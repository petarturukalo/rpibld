/*
 * Copyright (C) 2023 Petar Turukalo
 * SPDX-License-Identifier: GPL-2.0
 *
 * VideoCore mailbox tag/property interface.
 */
#ifndef VCMAILBOX_H
#define VCMAILBOX_H

#include "type.h"

enum tag_id {
	TAG_VC_GET_FIRMWARE_REV = 0x00000001,  /**< Get VideoCore firmware revision. */
	TAG_HW_GET_BOARD_REV    = 0x00010002,  /**< Get hardware board revision. */
	TAG_POWER_GET_STATE     = 0x00020001,
	TAG_CLOCK_GET_STATE     = 0x00030001,
	TAG_CLOCK_GET_RATE      = 0x00030002,
	TAG_GPIO_GET_STATE      = 0x00030041,  /**< Get GPIO expander pin state. */
	TAG_GPIO_GET_CONFIG     = 0x00030043   /**< Get GPIO expander pin config. */
};

enum vcmailbox_error {
	VCMBOX_ERROR_NONE,  /**< No error */
	/** The channel the tag requests were sent on didn't match the channel in the response to the request */
	VCMBOX_ERROR_RECEIVE_WRONG_CHANNEL,
	/** Internal response buffer address not the same as the internal request buffer address */
	VCMBOX_ERROR_BUFFER_RESPONSE_ADDR_MISMATCH,
	VCMBOX_ERROR_BUFFER_RESPONSE_BIT_NOT_SET,
	VCMBOX_ERROR_PARSING_REQUEST,  /**< VideoCore failed to parse internal request buffer */
	VCMBOX_ERROR_TAG_RESPONSE_BIT_NOT_SET,
	/**
	 * The size of the data returned by the VideoCore for a tag didn't match the expected/known 
	 * size of the tag's response. Note this means tags with a variable size response are not 
	 * currently supported.
	 */
	VCMBOX_ERROR_TAG_RESPONSE_SIZE_MISMATCH,
	/** The response tags are not in the original order that was passed to vcmailbox_request_tags() */
	VCMBOX_ERROR_TAG_RESPONSE_OUT_OF_ORDER,
	/** Not all of the tags originally requested in call to vcmailbox_request_tags() received a response */
	VCMBOX_ERROR_TAG_RESPONSE_MISSED
};

/**
 * @brief Data used to request a tag.
 */
struct tag_request {
	enum tag_id id;	/**< ID of tag to request */
	void *args;	/**< Arguments input to the request */
	int args_sz;	/**< Size of args buffer */
	void *ret;	/**< Where to store output of the request */
	int ret_sz;	/**< Size of ret buffer */
};

/**
 * @brief Request tags from the VideoCore.
 *
 * @param tag_requests Tags to request
 * @param n Number of tags requested
 */
enum vcmailbox_error vcmailbox_request_tags(struct tag_request *tag_requests, int n);

#endif
