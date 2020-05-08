/**
 *
 * \file util-internal.h
 * /brief getdns contect management functions
 *
 * This is the meat of the API
 * Originally taken from the getdns API description pseudo implementation.
 *
 */

/*
 * Copyright (c) 2013, NLnet Labs, Verisign, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the names of the copyright holders nor the
 *   names of its contributors may be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Verisign, Inc. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UTIL_INTERNAL_H
#define UTIL_INTERNAL_H

#include "config.h"
#include "context.h"
#include "rr-iter.h"
#include <ctype.h>

#define UNCONST_UINT8_p uint8_t *

#ifdef S_SPLINT_S
#  define INLINE 
#else
#  ifdef SWIG
#    define INLINE static
#  else
#    define INLINE static inline
#  endif
#endif

struct ub_result;
struct getdns_network_req;
getdns_return_t getdns_apply_network_result(getdns_network_req* netreq,
    int rcode, void *pkt, int pkt_len, int sec, char* why_bogus);

#define GETDNS_MAX_DNAME_LEN 255
#define GETDNS_MAX_LABEL_LEN 63

/**
 * add an item to the tail of a list - note that this was not in the getdns API
 * description but the list_set functions seem to be designed to modify an existing
 * item in the list.  The newly added item has no data type.
 * @param list list containing the item to which child_list is to be added
 * @param *child_dict dict to append to the list
 * @return GETDNS_RETURN_GOOD on success
 * @return GETDNS_RETURN_GENERAL_ERROR if out of memory
 */
getdns_return_t _getdns_list_append_this_dict(getdns_list *list,
    getdns_dict *child_dict);

getdns_return_t _getdns_list_append_dict(getdns_list *list,
    const getdns_dict *child_dict);

getdns_return_t _getdns_list_append_string(getdns_list *list,
    const char *value);

getdns_return_t _getdns_list_append_int(getdns_list *list,
    uint32_t child_uint32);

getdns_return_t _getdns_list_append_const_bindata(getdns_list *list,
    size_t size, const void *data);

getdns_return_t _getdns_dict_set_this_dict(getdns_dict *dict,
    const char *name, getdns_dict *child_dict);

getdns_return_t _getdns_dict_set_this_list(getdns_dict *dict,
    const char *name, getdns_list *child_list);

getdns_return_t _getdns_dict_set_const_bindata(getdns_dict *dict,
    const char *name, size_t size, const void *data);

getdns_return_t _getdns_dict_set_this_bindata(getdns_dict *dict,
    const char *name, getdns_bindata *child_bindata);

/**
  * private function (API users should not be calling this), this uses library
  * routines to make a copy of the list - would be faster to make the copy directly
  * caller must ensure that dstlist points to unallocated storage - the address will
  * be overwritten by a new list via a call to getdns_list_create(context)
  * @param srclist pointer to list to copy
  * @param dstlist pointer to pointer to list to receive the copy (will be allocated)
  * @return GETDNS_RETURN_GOOD on success
  * @return GETDNS_RETURN_NO_SUCH_LIST_ITEM if list is invalid
  * @return GETDNS_RETURN_GENERIC_ERROR if out of memory
  */
getdns_return_t _getdns_list_copy(const struct getdns_list *srclist,
    struct getdns_list **dstlist);

/**
 * private function used to make a copy of a dict structure, the caller is responsible
 * for freeing storage allocated to returned value
 * NOTE: not thread safe - this needs to be fixed to be thread safe
 * @param srcdict the dictionary structure to copy
 * @param dstdict pointer to the location to write pointer to new dictionary
 * @return GETDNS_RETURN_GOOD on success
 */
getdns_return_t
_getdns_dict_copy(const struct getdns_dict *srcdict,
    struct getdns_dict **dstdict);

/**
 * convert an ip address (v4/v6) dict to a sock storage
 * expects dict to contain keys GETDNS_STR_PORT, GETDNS_STR_ADDRESS_TYPE
 * GETDNS_STR_ADDRESS_DATA
 * @param ns pointer to dictionary containing keys listed above
 * @param output previously allocated storage used to return numeric address
 * @return GETDNS_RETURN_GOOD on success
 * @return GETDNS_RETURN_GENERIC_ERROR if keys missing from dictionary
 */
getdns_return_t _getdns_dict_to_sockaddr(struct getdns_dict * ns,
    struct sockaddr_storage *output);
getdns_return_t _getdns_sockaddr_to_dict(struct getdns_context *context,
    struct sockaddr_storage *sockaddr, struct getdns_dict ** output);

getdns_dict *
_getdns_rr_iter2rr_dict(struct mem_funcs *mf, _getdns_rr_iter *i);

getdns_dict *
_getdns_rr_iter2rr_dict_canonical(
    struct mem_funcs *mf, _getdns_rr_iter *i, uint32_t *orig_ttl);

struct getdns_dns_req;
struct getdns_dict *_getdns_create_getdns_response(
    struct getdns_dns_req *completed_request);

getdns_return_t _getdns_validate_dname(const char* dname);

uint8_t *_getdns_list2wire(const getdns_list *l,
    uint8_t *buf, size_t *buf_len, const struct mem_funcs *mf);

uint8_t *_getdns_reply2wire(const getdns_dict *r,
    uint8_t *buf, size_t *buf_len, const struct mem_funcs *mf);

void _getdns_wire2list(const uint8_t *pkt, size_t pkt_len, getdns_list *l);


/**
 * detect unrecognized extension strings or invalid extension formats
 * TODO: this could be optimized by searching a sorted list
 * @param extensions dictionary of valid extension strings and values
 * @return GETDNS_RETURN_GOOD if each extension string is valid and the format matches the API specification
 * @return GETDNS_RETURN_NO_SUCH_EXTENSION A name in the extensions dict is not a valid extension.
 * @return GETDNS_RETURN_EXTENSION_MISFORMAT One or more of the extensions has a bad format.
 */
getdns_return_t _getdns_validate_extensions(struct getdns_dict * extensions);

INLINE getdns_eventloop_event *getdns_eventloop_event_init(
    getdns_eventloop_event *ev,void *userarg, getdns_eventloop_callback read_cb,
    getdns_eventloop_callback write_cb, getdns_eventloop_callback timeout_cb)
{ ev->userarg = userarg; ev->read_cb = read_cb; ev->write_cb = write_cb;
  ev->timeout_cb = timeout_cb; ev->ev = NULL; return ev; }

#define GETDNS_CLEAR_EVENT(loop, event) \
	do { if ((event)->ev) (loop)->vmt->clear((loop), (event)); } while(0)
#define GETDNS_SCHEDULE_EVENT(loop, fd, timeout, event) \
	do { (loop)->vmt->schedule((loop),(fd),(timeout),(event)); } while(0)

INLINE void _dname_canonicalize(const uint8_t *src, uint8_t *dst)
{
	const uint8_t *next_label;

	while (*src) {
		next_label = src + *src + 1;
		*dst++ = *src++;
		while (src < next_label)
			*dst++ = (uint8_t)tolower((unsigned char)*src++);
	}
}

INLINE void _dname_canonicalize2(uint8_t *dname)
{
	_dname_canonicalize(dname, dname);
}

INLINE uint64_t _getdns_get_now_ms()
{
	struct timeval tv;

	(void) gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

INLINE uint64_t _getdns_ms_until_expiry(uint64_t expires)
{
	uint64_t now_ms = _getdns_get_now_ms();
	return now_ms >= expires ? 0 : expires - now_ms;
}

INLINE uint64_t _getdns_get_now_ms2(uint64_t *now_ms)
{
	struct timeval tv;

	if (!now_ms)	return _getdns_get_now_ms();
	if (*now_ms)	return *now_ms;

	(void) gettimeofday(&tv, NULL);
	return (*now_ms = (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

INLINE uint64_t _getdns_ms_until_expiry2(uint64_t expires, uint64_t *now_ms)
{
	if (*now_ms == 0) *now_ms = _getdns_get_now_ms();
	return *now_ms >= expires ? 0 : expires - *now_ms;
}

#endif
/* util-internal.h */
