#include "vcmailbox.h"
#include "mmio.h"
#include "help.h"
#include "heap.h"

enum vcmailbox_registers {
	MBOX0_READ,
	MBOX0_STATUS,
	MBOX1_WRITE,
	MBOX1_STATUS
};

static struct periph_access vcmailbox_access = {
	.periph_base_off = 0x200b880,
	.register_offsets = {
		[MBOX0_READ]   = 0x00,
		[MBOX0_STATUS] = 0x18,
		[MBOX1_WRITE]  = 0x20,
		[MBOX1_STATUS] = 0x38
	}
};

/*
 * Used to request and receive properties associated with a particular tag
 * from the VideoCore.
 *
 * @id: identifies which tag/property to get. Expected to store an enum tag_id.
 * @value_bufsz: size of value_buf in bytes, the greater between the bytes required 
 *	to store the request arguments and the bytes to store the response return 
 * @request/response_code: on request bit 31 should be clear. On response bit 31
 *	is set and bits 30:0 is the length of the value returned in value_buf.
 * @value_buf: when used in a request this stores the arguments input to the VC.
 *	When used in a response this stores the return value from the VC. 
 *	Both are stored at the start of this, regardless if their sizes differ, 
 *	e.g. if the args use 8 bytes and the response 4 bytes, the size of this will
 *	be 8 bytes, all 8 of which will be set to the args on request, but only the 
 *	first 4 bytes set on response (with the remaining 4 bytes unused).
 */
struct tag {
	uint32_t id;
	uint32_t value_bufsz;
	union {
		uint32_t request_code;
		uint32_t response_code;
	};
	uint8_t value_buf[];
/* Needs to be 32-bit aligned. */
} __attribute__((aligned(4)));

/*
 * Buffer used to request/receive properties from the VideoCore. 
 * Only to be used with channel CHANNEL_PROPERTY.
 *
 * @bufsz: size of this
 * @request/response_code: 0 on request. Bit 31 set on response with bit 0 identifying
 *	whether an error occurred: 0 for success, 1 for error.
 * @tags: list of properties/tags to get. This should be terminated with a zeroed uint32_t.
 */
struct property_buffer {
	uint32_t bufsz;
	union {
		uint32_t request_code;
		uint32_t response_code;
	};
	struct tag tags[];
} __attribute__((aligned(16)));

/*
 * Defines the type of a message sent through the VideoCore mailbox.
 *
 * @CHANNEL_PROPERTY: channel for the ARM to request properties/tags from
 *	the VideoCore. The data both sent and returned with this is a 16-byte aligned 
 *	address (i.e. the 4 least significant bits are clear) to a struct property_buffer.
 */
enum channel {
	CHANNEL_PROPERTY = 8
};

/*
 * @request_args_sz: size of buffer used to store arguments input
 *	to the tag's request
 * @response_sz: size of buffer used to store return value output 
 *	from the tag's response
 */
struct tag_io_size {
	enum tag_id id;
	int request_args_sz;
	int response_sz;
};

struct tag_io_size tag_io_sizes[] = {
#define EXPAND_TO(name, id, request_args_sz, response_sz) \
	{ id, request_args_sz, response_sz },
EXPAND_TAG_LIST
#undef EXPAND_TO
};

static struct tag_io_size *get_tag_io_size(enum tag_id id)
{
	struct tag_io_size *iosz = tag_io_sizes;
	int n = ARRAY_LEN(tag_io_sizes);

	for (int i = 0; i < n; ++i, ++iosz) {
		if (iosz->id == id)
			return iosz;
	}
	return NULL;
}

/* 
 * Channel spans the 4 least significant bits (bits 3:0) of the 32-bit message.
 */
#define CHANNEL_BITS 0b1111

/*
 * Write/send a message to the VideoCore.
 * @data: data in which the message encapsulates. The meaning of this is dependent
 *	on the channel being used (see enum channel for more info). Only the 28 most
 *	significant bits of this is used.
 */
static void vcmailbox_write_message(uint32_t data, enum channel chan)
{
	/* 
	 * Wait for full flag to clear (which after testing doesn't seem like is
	 * necessary, but do it anyway just in case). 
	 */
	while (register_get(&vcmailbox_access, MBOX1_STATUS)&1<<31)
		;
	register_set(&vcmailbox_access, MBOX1_WRITE, (data&(~CHANNEL_BITS))|chan);
}

/*
 * Read a response message to the most recent vcmailbox_write_message().
 * Returns the response data in the 28 most significant bits and the channel 
 * in the 4 least significant bits.
 */
static uint32_t vcmailbox_read_message(void)
{
	/* 
	 * Wait for empty flag to clear (opposed to waiting for an interrupt; this is 
	 * necessary compared to the wait done in vcmailbox_write_message()). 
	 * */
	while (register_get(&vcmailbox_access, MBOX0_STATUS)&1<<30)
		;
	return register_get(&vcmailbox_access, MBOX0_READ);
}

/*
 * Return addr aligned to the nearest n-byte aligned address greater than or equal to itself.
 */
static void *align_address(void *addr, int n)
{
	while ((int)addr%n)
		++addr;
	return addr;
}

/*
 * Build up a variable-sized tag.
 *
 * @tag: heap address of tag to build up
 * @tag_request: data used to build up the tag. Its tag_io_data 
 *	stores the tag request arguments, if any
 *
 * Return the size of the built tag in bytes, or -1 on error if the tag
 * is unimplemented.
 */
static int build_tag(struct tag *tag, struct tag_request *req)
{
	struct tag_io_size *iosz;
	/* Used to build up the tag. Points to the next available address. */
	byte_t *tag_end;
	
	tag->id = req->id;
	iosz = get_tag_io_size(tag->id);
	if (!iosz) 
		return -1;
	tag->value_bufsz = max(iosz->request_args_sz, iosz->response_sz);
	tag->request_code = 0;

	tag_end = (byte_t *)&tag->value_buf + tag->value_bufsz;
	mzero(&tag->value_buf, tag->value_bufsz);
	if (iosz->request_args_sz) {
		/* Copy request arguments into value buffer. */
		mcopy(req->tag_io_data, &tag->value_buf, iosz->request_args_sz);
	}
	/* 32-bit align. */
	tag_end = align_address(tag_end, 4);
	return tag_end - (byte_t *)tag;
}

/*
 * Build up a property buffer on the heap and return the address to it.
 * The heap is used because the property buffer and tags are variable size 
 * dependent on which and how many tags are selected.
 *
 * Return NULL if any one requested tag is unimplemented.
 */
static struct property_buffer *build_property_buffer(struct tag_request *tag_requests, int n)
{
	struct property_buffer *prop;  
	/* Used to build up the property buffer. Points to the next available address. */
	byte_t *prop_end;  
	struct tag_request *req;
	int tag_size;

	prop = heap_get_base_address();
	prop = align_address(prop, 16);

	prop->request_code = 0;
	prop_end = (byte_t *)&prop->tags;
	/* Build tags. */
	req = tag_requests;
	for (int i = 0; i < n; ++i, ++req) {
		tag_size = build_tag((struct tag *)prop_end, req);
		if (tag_size == -1)
			return NULL;
		prop_end += tag_size;
	}
	/* Mark end of tags. */
	*(uint32_t *)prop_end = 0;
	prop_end += sizeof(uint32_t);

	prop_end = align_address(prop_end, 16);
	prop->bufsz = prop_end - (byte_t *)prop;
	return prop;
}

/*
 * Copy the tag responses into the user's output buffers.
 */
static enum vcmailbox_error return_tag_responses(struct property_buffer *prop, 
						 struct tag_request *tag_requests, int n)
{
	struct tag *tag = (struct tag *)&prop->tags;
	struct tag_request *req = tag_requests;
	struct tag_io_size *iosz;
	int i = 0;

	/* Will stop at the zeroed "end tag" marker. */
	for (; tag->id; ++req, ++i) {
		/* Validation. */
		if (tag->id != req->id)
			/* Or got a response from a tag that wasn't supposed to be there. */
			return VCMBOX_ERROR_TAG_RESPONSE_OUT_OF_ORDER;
		if (!(tag->response_code&1<<31))
			return VCMBOX_ERROR_TAG_RESPONSE_BIT_NOT_SET;
		iosz = get_tag_io_size(tag->id);
		if (iosz->response_sz != (tag->response_code&~(1<<31)))
			return VCMBOX_ERROR_TAG_RESPONSE_SIZE_MISMATCH;

		/* Copy tag response into user's output buffer. */
		mcopy(&tag->value_buf, req->tag_io_data+iosz->request_args_sz, iosz->response_sz);

		/* Jump to next tag. */
		tag = (struct tag *)((byte_t *)&tag->value_buf + tag->value_bufsz);
	}
	if (i != n)
		return VCMBOX_ERROR_TAG_RESPONSE_MISSED;
	return VCMBOX_ERROR_NONE;
}

enum vcmailbox_error vcmailbox_request_tags(struct tag_request *tag_requests, int n)
{
	struct property_buffer *send_prop, *recv_prop;
	uint32_t recv_msg;

	send_prop = build_property_buffer(tag_requests, n);
	if (!send_prop) 
		return VCMBOX_ERROR_TAG_UNIMPLEMENTED;

	vcmailbox_write_message((uint32_t)send_prop, CHANNEL_PROPERTY);
	recv_msg = vcmailbox_read_message();
	if (recv_msg&CHANNEL_BITS != CHANNEL_PROPERTY)
		return VCMBOX_ERROR_RECEIVE_WRONG_CHANNEL;
	recv_prop = (struct property_buffer *)(recv_msg&(~CHANNEL_BITS));

	if (recv_prop != send_prop)
		return VCMBOX_ERROR_BUFFER_RESPONSE_ADDR_MISMATCH;
	if (!(recv_prop->response_code&1<<31)) 
		return VCMBOX_ERROR_BUFFER_RESPONSE_BIT_NOT_SET;
	if (recv_prop->response_code&1) 
		return VCMBOX_ERROR_PARSING_REQUEST;

	return return_tag_responses(recv_prop, tag_requests, n);
}

