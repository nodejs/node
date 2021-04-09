/**
 *
 * /brief getdns list management functions
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

#include <string.h>
#include <ctype.h>
#include "types-internal.h"
#include "util-internal.h"
#include "list.h"
#include "dict.h"

getdns_return_t
_getdns_list_find(const getdns_list *list, const char *key, getdns_item **item)
{
	const char *next;
	char *endptr;
	size_t index;
	getdns_item *i;

	if (*key == '/') {
		if (!(next = strchr(++key, '/')))
			next = strchr(key, '\0');
	} else
		next = strchr(key, '\0');

	if (key[0] == '-' && next == key + 1)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	index = strtoul(key, &endptr, 10);
	if (!isdigit((int)*key) || endptr != next)
		/* Not a list index, so it was assumed */
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	if (index >= list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	i = &list->items[index];
	if (!*next) {
		*item = i;
		return GETDNS_RETURN_GOOD;

	} else switch (i->dtype) {
	case t_dict: return _getdns_dict_find(i->data.dict, next, item);
	case t_list: return _getdns_list_find(i->data.list, next, item);
	default    : /* Trying to dereference a non list or dict */
	             return GETDNS_RETURN_NO_SUCH_LIST_ITEM;
	}
}


getdns_return_t
_getdns_list_remove_name(getdns_list *list, const char *name)
{
	const char *next, *key = name;
	char *endptr;
	size_t index;
	getdns_item *i;

	if (*key == '/') {
		if (!(next = strchr(++key, '/')))
			next = strchr(key, '\0');
	} else
		next = strchr(key, '\0');

	if (key[0] == '-' && next == key + 1)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	index = strtoul(key, &endptr, 10);
	if (!isdigit((int)*key) || endptr != next)
		/* Not a list index, so it was assumed */
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	if (index >= list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	i = &list->items[index];
	if (!*next) {
		switch (i->dtype) {
		case t_dict   : getdns_dict_destroy(i->data.dict); break;
		case t_list   : getdns_list_destroy(i->data.list); break;
		case t_bindata: _getdns_bindata_destroy(
		                    &list->mf, i->data.bindata);
		default       : break;
		}
		if (index < list->numinuse - 1)
			(void) memmove( i, &i[1],
			    (list->numinuse - index) * sizeof(getdns_item));
		list->numinuse -= 1;
		return GETDNS_RETURN_GOOD;

	} else switch (i->dtype) {
	case t_dict: return getdns_dict_remove_name(i->data.dict, next);
	case t_list: return _getdns_list_remove_name(i->data.list, next);
	default    : /* Trying to dereference a non list or dict */
	             return GETDNS_RETURN_NO_SUCH_LIST_ITEM;
	}
}


getdns_return_t
_getdns_list_find_and_add(
    getdns_list *list, const char *key, getdns_item **item)
{
	const char *next;
	char *endptr;
	size_t index;
	getdns_item *newlist, *i;

	if (*key == '/') {
		if (!(next = strchr(++key, '/')))
			next = strchr(key, '\0');
	} else
		next = strchr(key, '\0');

	if (key[0] == '-' && next == key + 1)
		index = list->numinuse;
	else {
		index = strtoul(key, &endptr, 10);
		if (!isdigit((int)*key) || endptr != next)
			/* Not a list index, so it was assumed */
			return GETDNS_RETURN_WRONG_TYPE_REQUESTED;
	}
	if (index > list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	if (index == list->numinuse) {
		if (list->numalloc <= list->numinuse) {
			if (!(newlist = GETDNS_XREALLOC(list->mf, list->items,
			    getdns_item, list->numalloc+GETDNS_LIST_BLOCKSZ)))
				return GETDNS_RETURN_MEMORY_ERROR;
			list->items = newlist;
			list->numalloc += GETDNS_LIST_BLOCKSZ;
		}
		list->numinuse++;
		i = &list->items[index];

		if (!*next) {
			i->dtype = t_int;
			i->data.n = 55555333;
			*item = i;
			return GETDNS_RETURN_GOOD;
		}
		if ((next[1] == '0' || next[1] == '-') &&
		    (next[2] == '/' || next[2] == '\0')) {

			i->dtype = t_list;
			i->data.list = _getdns_list_create_with_mf(&list->mf);
			return _getdns_list_find_and_add(
			    i->data.list, next, item);
		}
		i->dtype = t_dict;
		i->data.dict = _getdns_dict_create_with_mf(&list->mf);
		return _getdns_dict_find_and_add(i->data.dict, next, item);
	}
	i = &list->items[index];
	if (!*next) {
		switch (i->dtype) {
		case t_dict   : getdns_dict_destroy(i->data.dict); break;
		case t_list   : getdns_list_destroy(i->data.list); break;
		case t_bindata: _getdns_bindata_destroy(
		                      &list->mf, i->data.bindata); break;
		default       : break;
		}
		i->dtype = t_int;
		i->data.n = 33355555;
		*item = i;
		return GETDNS_RETURN_GOOD;

	} else switch (i->dtype) {
	case t_dict: return _getdns_dict_find_and_add(i->data.dict,next,item);
	case t_list: return _getdns_list_find_and_add(i->data.list,next,item);
	default    : /* Trying to dereference a non list or dict */
	             return GETDNS_RETURN_WRONG_TYPE_REQUESTED;
	}
}



/*---------------------------------------- getdns_list_get_length */
getdns_return_t
getdns_list_get_length(const struct getdns_list * list, size_t * answer)
{
	if (!list || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	*answer = list->numinuse;
	return GETDNS_RETURN_GOOD;;
}				/* getdns_list_get_length */

/*---------------------------------------- getdns_list_get_data_type */
getdns_return_t
getdns_list_get_data_type(const struct getdns_list * list, size_t index,
	getdns_data_type * answer)
{
	if (!list || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (index >= list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	*answer = list->items[index].dtype;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_get_data_type */

/*---------------------------------------- getdns_list_get_dict */
getdns_return_t
getdns_list_get_dict(const struct getdns_list * list, size_t index,
	struct getdns_dict ** answer)
{
	if (!list || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (index >= list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	if (list->items[index].dtype != t_dict)
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	*answer = list->items[index].data.dict;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_get_dict */

/*---------------------------------------- getdns_list_get_list */
getdns_return_t
getdns_list_get_list(const struct getdns_list * list, size_t index,
	struct getdns_list ** answer)
{
	if (!list || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (index >= list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	if (list->items[index].dtype != t_list)
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	*answer = list->items[index].data.list;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_get_list */

/*---------------------------------------- getdns_list_get_bindata */
getdns_return_t
getdns_list_get_bindata(const struct getdns_list * list, size_t index,
	struct getdns_bindata ** answer)
{

	if (!list || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (index >= list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	if (list->items[index].dtype != t_bindata)
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	*answer = list->items[index].data.bindata;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_get_bindata */

/*---------------------------------------- getdns_list_get_int */
getdns_return_t
getdns_list_get_int(const struct getdns_list * list, size_t index,
	uint32_t * answer)
{
	if (!list || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (index >= list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	if (list->items[index].dtype != t_int)
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	*answer = list->items[index].data.n;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_get_int */

/*---------------------------------------- _getdns_list_copy */
getdns_return_t
_getdns_list_copy(const struct getdns_list * srclist,
	struct getdns_list ** dstlist)
{
	size_t i;
	getdns_return_t retval;

	if (!dstlist)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!srclist) {
		*dstlist = NULL;
		return GETDNS_RETURN_GOOD;
	}
	*dstlist = getdns_list_create_with_extended_memory_functions(
		srclist->mf.mf_arg,
		srclist->mf.mf.ext.malloc,
		srclist->mf.mf.ext.realloc,
		srclist->mf.mf.ext.free
	);
	if (!*dstlist)
		return GETDNS_RETURN_MEMORY_ERROR;

	for (i = 0; i < srclist->numinuse; i++) {
		switch (srclist->items[i].dtype) {
		case t_int:
			retval = _getdns_list_append_int(*dstlist,
			    srclist->items[i].data.n);
			break;

		case t_list:
			retval = getdns_list_set_list(*dstlist,
			    (*dstlist)->numinuse, srclist->items[i].data.list);
			break;

		case t_bindata:
			retval = _getdns_list_append_const_bindata(*dstlist,
			    srclist->items[i].data.bindata->size, 
			    srclist->items[i].data.bindata->data);
			break;

		case t_dict:
			retval = _getdns_list_append_dict(*dstlist,
			    srclist->items[i].data.dict);
			break;
		default:
			retval = GETDNS_RETURN_WRONG_TYPE_REQUESTED;
			break;
		}
		if (retval != GETDNS_RETURN_GOOD) {
			getdns_list_destroy(*dstlist);
			*dstlist = NULL;
			return retval;
		}
	}
	return GETDNS_RETURN_GOOD;
}				/* _getdns_list_copy */

struct getdns_list *
getdns_list_create_with_extended_memory_functions(
	void *userarg,
	void *(*malloc)(void *userarg, size_t),
	void *(*realloc)(void *userarg, void *, size_t),
	void (*free)(void *userarg, void *))
{
	struct getdns_list *list;
	mf_union mf;

	if (!malloc || !realloc || !free)
		return NULL;

	mf.ext.malloc = malloc;
	list = userarg == MF_PLAIN
		 ? (struct getdns_list *)(*mf.pln.malloc)(
			   sizeof(struct getdns_list))
		 : (struct getdns_list *)(*mf.ext.malloc)(userarg,
			   sizeof(struct getdns_list));
	if (!list)
		return NULL;

	list->mf.mf_arg         = userarg;
	list->mf.mf.ext.malloc  = malloc;
	list->mf.mf.ext.realloc = realloc;
	list->mf.mf.ext.free    = free;

	list->numinuse = 0;
	if (!(list->items = GETDNS_XMALLOC(
	    list->mf, getdns_item, GETDNS_LIST_BLOCKSZ))) {
		GETDNS_FREE(list->mf, list);
		return NULL;
	}
	list->numalloc = GETDNS_LIST_BLOCKSZ;
	return list;
}

struct getdns_list *
getdns_list_create_with_memory_functions(void *(*malloc)(size_t),
	void *(*realloc)(void *, size_t), void (*free)(void *))
{
	mf_union mf;
	mf.pln.malloc = malloc;
	mf.pln.realloc = realloc;
	mf.pln.free = free;
	return getdns_list_create_with_extended_memory_functions(
		MF_PLAIN, mf.ext.malloc, mf.ext.realloc, mf.ext.free);
}


/*-------------------------- getdns_list_create_with_context */
struct getdns_list *
getdns_list_create_with_context(const getdns_context *context)
{
	if (context)
		return getdns_list_create_with_extended_memory_functions(
			context->mf.mf_arg,
			context->mf.mf.ext.malloc,
			context->mf.mf.ext.realloc,
			context->mf.mf.ext.free
		);
	else
		return getdns_list_create_with_memory_functions(malloc,
			realloc, free);
}			/* getdns_list_create_with_context */

/*---------------------------------------- getdns_list_create */
struct getdns_list *
getdns_list_create()
{
	return getdns_list_create_with_context(NULL);
}				/* getdns_list_create */

static void
_getdns_list_destroy_item(struct getdns_list *list, size_t index)
{
	switch (list->items[index].dtype) {
	case t_dict:
		getdns_dict_destroy(list->items[index].data.dict);
		break;

	case t_list:
		getdns_list_destroy(list->items[index].data.list);
		break;

	case t_bindata:
		_getdns_bindata_destroy(&list->mf,
			list->items[index].data.bindata);
		break;
	default:
		break;
	}
}

/*---------------------------------------- getdns_list_destroy */
void
getdns_list_destroy(struct getdns_list *list)
{
	size_t i;

	if (!list)
		return;

	for (i = 0; i < list->numinuse; i++)
		_getdns_list_destroy_item(list, i);

	if (list->items)
		GETDNS_FREE(list->mf, list->items);
	GETDNS_FREE(list->mf, list);
}				/* getdns_list_destroy */


static getdns_return_t
_getdns_list_request_index(getdns_list *list, size_t index)
{
	getdns_item *newlist;

	assert(list);

	if (index > list->numinuse)
		return GETDNS_RETURN_NO_SUCH_LIST_ITEM;

	if (index < list->numinuse) {
		_getdns_list_destroy_item(list, index);
		return GETDNS_RETURN_GOOD;

	}
	if (list->numalloc > list->numinuse) {
		list->numinuse++;
		return GETDNS_RETURN_GOOD;
	}
	if (!(newlist = GETDNS_XREALLOC(list->mf, list->items,
	    getdns_item, list->numalloc + GETDNS_LIST_BLOCKSZ)))

		return GETDNS_RETURN_MEMORY_ERROR;

	list->numinuse++;
	list->items = newlist;
	list->numalloc += GETDNS_LIST_BLOCKSZ;

	return GETDNS_RETURN_GOOD;
}

/*---------------------------------------- getdns_list_set_dict */
static getdns_return_t
_getdns_list_set_this_dict(
    getdns_list *list, size_t index, getdns_dict *child_dict)
{
	getdns_return_t r;

	if (!list || !child_dict)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_list_request_index(list, index)))
		return r;

	list->items[index].dtype = t_dict;
	list->items[index].data.dict = child_dict;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_set_dict */

getdns_return_t
getdns_list_set_dict(
    getdns_list *list, size_t index, const getdns_dict *child_dict)
{
	getdns_dict *newdict;
	getdns_return_t r;

	if (!list || !child_dict)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_copy(child_dict, &newdict)))
		return r;

	if ((r = _getdns_list_set_this_dict(list, index, newdict)))
		getdns_dict_destroy(newdict);

	return r;
}				/* getdns_list_set_dict */

/*---------------------------------------- getdns_list_set_list */
getdns_return_t
getdns_list_set_list(
    getdns_list *list, size_t index, const getdns_list *child_list)
{
	getdns_list *newlist;
	getdns_return_t r;

	if (!list || !child_list)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_list_copy(child_list, &newlist)))
		return r;

	if ((r = _getdns_list_request_index(list, index))) {
		getdns_list_destroy(newlist);
		return r;
	}
	list->items[index].dtype = t_list;
	list->items[index].data.list = newlist;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_set_list */

/*---------------------------------------- getdns_list_set_bindata */
static getdns_return_t
_getdns_list_set_const_bindata(
    getdns_list *list, size_t index, size_t size, const void *data)
{
	getdns_bindata *newbindata;
	getdns_return_t r;

	if (!list)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!(newbindata = _getdns_bindata_copy(&list->mf, size, data)))
		return GETDNS_RETURN_MEMORY_ERROR;

	if ((r = _getdns_list_request_index(list, index))) {
		_getdns_bindata_destroy(&list->mf, newbindata);
		return r;
	}
	list->items[index].dtype = t_bindata;
	list->items[index].data.bindata = newbindata;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_set_bindata */

getdns_return_t
getdns_list_set_bindata(
    getdns_list *list, size_t index, const getdns_bindata *child_bindata)
{
	return !child_bindata ? GETDNS_RETURN_INVALID_PARAMETER
	    : _getdns_list_set_const_bindata(
	    list, index, child_bindata->size, child_bindata->data);
}

/*----------------------------------------- getdns_list_set_string */
static getdns_return_t
getdns_list_set_string(getdns_list *list, size_t index, const char *value)
{
	getdns_bindata *newbindata;
	getdns_return_t r;

	if (!list || !value)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!(newbindata = _getdns_bindata_copy(
	    &list->mf, strlen(value) + 1, (uint8_t *)value)))
		return GETDNS_RETURN_MEMORY_ERROR;

	newbindata->size -= 1;

	if ((r = _getdns_list_request_index(list, index))) {
		_getdns_bindata_destroy(&list->mf, newbindata);
		return r;
	}
	list->items[index].dtype = t_bindata;
	list->items[index].data.bindata = newbindata;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_set_string */

/*---------------------------------------- getdns_list_set_int */
getdns_return_t
getdns_list_set_int(getdns_list * list, size_t index, uint32_t child_int)
{
	getdns_return_t r;

	if (!list)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_list_request_index(list, index)))
		return r;

	list->items[index].dtype = t_int;
	list->items[index].data.n = child_int;
	return GETDNS_RETURN_GOOD;
}				/* getdns_list_set_int */

getdns_return_t
_getdns_list_append_dict(getdns_list *list, const getdns_dict *child_dict)
{
	if (!list) return GETDNS_RETURN_INVALID_PARAMETER;
	return getdns_list_set_dict(list, list->numinuse, child_dict);
}
getdns_return_t
_getdns_list_append_this_dict(getdns_list *list, getdns_dict *child_dict)
{
	if (!list) return GETDNS_RETURN_INVALID_PARAMETER;
	return _getdns_list_set_this_dict(list, list->numinuse, child_dict);
}
getdns_return_t
_getdns_list_append_const_bindata(
    getdns_list *list, size_t size, const void *data)
{
	if (!list) return GETDNS_RETURN_INVALID_PARAMETER;
	return _getdns_list_set_const_bindata(list, list->numinuse, size, data);
}
getdns_return_t
_getdns_list_append_string(getdns_list *list, const char *value)
{
	if (!list) return GETDNS_RETURN_INVALID_PARAMETER;
	return getdns_list_set_string(list, list->numinuse, value);
}
getdns_return_t
_getdns_list_append_int(getdns_list *list, uint32_t child_int)
{
	if (!list) return GETDNS_RETURN_INVALID_PARAMETER;
	return getdns_list_set_int(list, list->numinuse, child_int);
}
/* getdns_list.c */

