/*
 * VideoCore mailbox tag/property interface.
 * To implement a new tag for use by vcmailbox_request_tags() add 
 * its ID to enum tag_id, and add an entry for it to EXPAND_TAG_LIST.
 */
#ifndef VCMAILBOX_H
#define VCMAILBOX_H

#include "type.h"

enum tag_id {
	VC_GET_FIRMWARE_REV = 0x00000001,  /* Get VideoCore firmware revision. */
	HW_GET_BOARD_REV    = 0x00010002,  /* Get hardware board revision. */
	POWER_GET_STATE     = 0x00020001,
	CLOCK_GET_STATE     = 0x00030001,
	CLOCK_GET_RATE      = 0x00030002,
	CLOCK_SET_RATE      = 0x00038002,
	GPIO_GET_STATE      = 0x00030041,  /* Get GPIO expander pin state. */
	GPIO_SET_STATE      = 0x00038041,  /* Set GPIO expander pin state. */
	GPIO_GET_CONFIG     = 0x00030043,  /* Get GPIO expander pin config. */
	GPIO_SET_CONFIG     = 0x00038043   /* Set GPIO expander pin config. */ // TODO rm?
};

/*
 * List of tags and the tag metadata needed by a user of this interface.
 * This exists to generate code in several different places that require the
 * same data, so that this "same data" is only defined in one place (here).
 *
 * The parameters to EXPAND_TO, from left to right, are the following.
 * 1. name of tag to use in generated identifier
 * 2. the tag's ID, an enum tag_id
 * 3. bytes required to store tag request arguments
 * 4. bytes required to store tag response return value
 *
 * TODO check what return of set state is? might be safe to just put it as 4 or 8
 */
#define EXPAND_TAG_LIST \
	EXPAND_TO(vc_get_firmware_rev, VC_GET_FIRMWARE_REV, 0,  4) \
	EXPAND_TO(hw_get_board_rev,    HW_GET_BOARD_REV,    0,  4) \
	EXPAND_TO(power_get_state,     POWER_GET_STATE,     4,  8) \
	EXPAND_TO(clock_get_state,     CLOCK_GET_STATE,     4,  8) \
	EXPAND_TO(clock_get_rate,      CLOCK_GET_RATE,      4,  8) \
	EXPAND_TO(clock_set_rate,      CLOCK_SET_RATE,      12, 8) \
	EXPAND_TO(gpio_get_state,      GPIO_GET_STATE,      8,  8) \
	EXPAND_TO(gpio_set_state,      GPIO_SET_STATE,      8,  0) \
	EXPAND_TO(gpio_get_config,     GPIO_GET_CONFIG,     4,  20) \
	EXPAND_TO(gpio_set_config,     GPIO_SET_CONFIG,     24, 4)

/*
 * For each tag generate a struct definition named tag_io_<name>, the struct
 * that is pointed to by tag_request.tag_io_data.
 *
 * @args: where to store tag request arguments
 * @ret: where tag response return value is stored
 *
 * This exists so the user of this interface can declare locally inputs and outputs to a tag request
 * without having to know or explicitly define the input/output buffer sizes each time a tag request 
 * is made. 
 * TODO 
 * - is this something the user wants, or will they need to know the size? can also use
 * getters/setters
 * - could also just make a interface for a tag's I/O reqs and make it define the arguments to the
 *   request and return value as well (could do a wrapper function)
 * - can implement a tag as a tag ID and a function pointer to a fn used to call the tag
 * -- struct { ID (not enum), fn pointer } TODO but then how will the internals know about the sizes
 * - TODO only need one buf since args will get overwritten?
 */
#define EXPAND_TO(name, id, args_sz, ret_sz) \
	struct tag_io_##name { \
		uint8_t args[args_sz]; \
		uint8_t ret[ret_sz]; \
	};
EXPAND_TAG_LIST
#undef EXPAND_TO

/*
 * @VCMBOX_ERROR_NONE: no error
 * @VCMBOX_ERROR_TAG_UNIMPLEMENTED: a tag is not yet supported by this interface
 * @VCMBOX_ERROR_RECEIVE_WRONG_CHANNEL: the channel the tag requests were sent on didn't
 *	match the channel in the response to the request
 * @VCMBOX_ERROR_BUFFER_RESPONSE_ADDR_MISMATCH: internal response buffer address not the 
 *	same as the internal request buffer address
 * @VCMBOX_ERROR_PARSING_REQUEST: VideoCore failed to parse internal request buffer
 * @VCMBOX_ERROR_TAG_RESPONSE_SIZE_MISMATCH: the size of the data returned by the 
 *	VideoCore for a tag didn't match the expected/known size of the tag's response.
 *	Note this means tags with a variable size response are not currently supported.
 * @VCMBOX_ERROR_TAG_RESPONSE_OUT_OF_ORDER: the response tags are not in the original 
 *	order that was passed to vcmailbox_request_tags()
 * @VCMBOX_ERROR_TAG_RESPONSE_MISSED: not all of the tags originally requested in call
 *	to vcmailbox_request_tags() received a response
 */
enum vcmailbox_error {
	VCMBOX_ERROR_NONE,
	VCMBOX_ERROR_TAG_UNIMPLEMENTED,
	VCMBOX_ERROR_RECEIVE_WRONG_CHANNEL,
	VCMBOX_ERROR_BUFFER_RESPONSE_ADDR_MISMATCH,
	VCMBOX_ERROR_BUFFER_RESPONSE_BIT_NOT_SET,
	VCMBOX_ERROR_PARSING_REQUEST,
	VCMBOX_ERROR_TAG_RESPONSE_BIT_NOT_SET,
	VCMBOX_ERROR_TAG_RESPONSE_SIZE_MISMATCH,
	VCMBOX_ERROR_TAG_RESPONSE_OUT_OF_ORDER,
	VCMBOX_ERROR_TAG_RESPONSE_MISSED
};

/*
 * @tag_io_data: pointer to one of generated struct tag_io_<name> above corresponding to 
 *	the given tag ID. If the tag takes arguments the args field must be filled 
 *	before call to vcmailbox_request_tags(). If the tag has a return value on return 
 *	from vcmailbox_request_tags() the ret field will be filled with the return value. 
 */
struct tag_request {
	enum tag_id id;
	void *tag_io_data;
};

/*
 * Request tags from the VideoCore.
 *
 * @tag_requests: tags to request
 * @n: number of tags requested
 *
 * TODO getters and setters for tag_io_<name>? or at least expose the sizes here in the header
 * - if getters/setters then pass in a struct <tag ID, tag_io_data>
 */
enum vcmailbox_error vcmailbox_request_tags(struct tag_request *tag_requests, int n);

#endif
