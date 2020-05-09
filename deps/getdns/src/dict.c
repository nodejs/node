/**
 *
 * getdns dict management functions, note that the internal storage is
 * accomplished via an _getdns_rbtree_t
 *
 * Interfaces originally taken from the getdns API description pseudo implementation.
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

#include "config.h"
#include <ctype.h>
#ifndef USE_WINSOCK
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "types-internal.h"
#include "util-internal.h"
#include "dict.h"
#include "list.h"
#include "rr-dict.h"
#include "const-info.h"
#include "gldns/gbuffer.h"
#include "gldns/wire2str.h"
#include "gldns/parseutil.h"


static char *_json_ptr_first(const struct mem_funcs *mf,
    const char *jptr, char *first, ssize_t first_sz)
{
	const char *next_ref, *k;
	char *j;

	if (*jptr != '/')
		return (char *)jptr;
	jptr++;
	if (!(next_ref = strchr(jptr, '/')))
		next_ref = strchr(jptr, '\0');

	if ((ssize_t)(next_ref - jptr + 1) > first_sz || !first)
		first = GETDNS_XMALLOC(*mf, char, next_ref - jptr + 1);

	for (j = first, k = jptr; k < next_ref; j++, k++)
		*j = k[0] == '~' && k[1] == '1' ? (k++, '/') : *k;
	*j = '\0';
	for (j = first, k = first; *k; j++, k++)
		*j = k[0] == '~' && k[1] == '0' ? (k++, '~') : *k;
	*j = '\0';

	return first;
}


static struct getdns_dict_item *
_find_dict_item(const getdns_dict *dict, const char *jptr)
{
	char first_spc[1024] = "", *first;
	struct getdns_dict_item *d;

	first = _json_ptr_first(&dict->mf, jptr,
	    first_spc, sizeof(first_spc));

	d = (struct getdns_dict_item *) _getdns_rbtree_search(
	    (_getdns_rbtree_t *)&(dict->root), first);

	if (first && first != jptr && first != first_spc)
		GETDNS_FREE(dict->mf, first);

	return d;
}


static struct getdns_dict_item *
_delete_dict_item(const getdns_dict *dict, const char *jptr)
{
	char first_spc[1024], *first;
	struct getdns_dict_item *d;

	first = _json_ptr_first(&dict->mf, jptr,
	    first_spc, sizeof(first_spc));

	d = (struct getdns_dict_item *) _getdns_rbtree_delete(
	    (_getdns_rbtree_t *)&(dict->root), first);

	if (first && first != jptr && first != first_spc)
		GETDNS_FREE(dict->mf, first);

	return d;
}


static char *
_json_ptr_keydup(const struct mem_funcs *mf, const char *jptr)
{
	char *first = _json_ptr_first(mf, jptr, NULL, 0);

	if (first == jptr || first == jptr + 1) {
		size_t sz = strlen(jptr) + 1;

		if ((first = GETDNS_XMALLOC(*mf, char, sz)))
			memcpy(first, jptr, sz);
	}
	return first;
}


getdns_return_t
_getdns_dict_find(const getdns_dict *dict, const char *key, getdns_item **item)
{
	const char *next;
	struct getdns_dict_item *d;
	
	if (!(d = _find_dict_item(dict, key)))
		return GETDNS_RETURN_NO_SUCH_DICT_NAME;

	if (*key != '/' || !(next = strchr(key + 1, '/'))) {
		*item = &d->i;
		return GETDNS_RETURN_GOOD;

	} else switch (d->i.dtype) { /* Key was nested reference */
	case t_dict: return _getdns_dict_find(d->i.data.dict, next, item);
	case t_list: return _getdns_list_find(d->i.data.list, next, item);
	default    : /* Trying to dereference a non list or dict */
	             return GETDNS_RETURN_WRONG_TYPE_REQUESTED;
	}
}


/*---------------------------------------- getdns_dict_item_free */
/**
 * private function used to release storage associated with a dictionary item
 * @param node all memory in this structure and its children will be freed
 * @param arg is a dict who's custom memory function will be used 
 *            to free the items
 * @return void
 */
static void
getdns_dict_item_free(_getdns_rbnode_t *node, void *arg)
{
	struct getdns_dict_item *d = (struct getdns_dict_item *)node;
	struct getdns_dict *dict = (struct getdns_dict *)arg;

	assert(node);
	assert(arg);

	switch (d->i.dtype) {
	case t_dict   : getdns_dict_destroy(d->i.data.dict); break;
	case t_list   : getdns_list_destroy(d->i.data.list); break;
	case t_bindata: _getdns_bindata_destroy(&dict->mf, d->i.data.bindata);
	default       : break;
	}
	if (node->key)
		GETDNS_FREE(dict->mf, (void *)node->key);
	GETDNS_FREE(dict->mf, node);
}				/* getdns_dict_item_free */


getdns_return_t
getdns_dict_remove_name(getdns_dict *dict, const char *name)
{
	const char *next;
	struct getdns_dict_item *d;

	if (!dict || !name)
		return GETDNS_RETURN_INVALID_PARAMETER;
	
	if (!(d = _find_dict_item(dict, name)))
		return GETDNS_RETURN_NO_SUCH_DICT_NAME;

	if (*name != '/' || !(next = strchr(name + 1, '/'))) {
		d = _delete_dict_item(dict, name);
		getdns_dict_item_free(&d->node, dict);
		return GETDNS_RETURN_GOOD;

	} else switch (d->i.dtype) {
	case t_dict: return getdns_dict_remove_name(d->i.data.dict, next);
	case t_list: return _getdns_list_remove_name(d->i.data.list, next);
	default    : /* Trying to dereference a non list or dict */
	             return GETDNS_RETURN_WRONG_TYPE_REQUESTED;
	}
}


getdns_return_t
_getdns_dict_find_and_add(
    getdns_dict *dict, const char *key, getdns_item **item)
{
	const char *next;
	struct getdns_dict_item *d;

	if (!(d = _find_dict_item(dict, key))) {
		/* add a node */
		d = GETDNS_MALLOC(dict->mf, struct getdns_dict_item);
		d->node.key = _json_ptr_keydup(&dict->mf, key);
		_getdns_rbtree_insert(&(dict->root), (_getdns_rbnode_t *) d);
		if (*key != '/' || !(next = strchr(key + 1, '/'))) {
			(void) memset(&d->i.data, 0, sizeof(d->i.data));
			d->i.dtype = t_int;
			d->i.data.n = 55555333;
			*item = &d->i;
			return GETDNS_RETURN_GOOD;
		}
		if ((next[1] == '0' || next[1] == '-') && 
		    (next[2] == '/' || next[2] == '\0')) {

			d->i.dtype = t_list;
			d->i.data.list =_getdns_list_create_with_mf(&dict->mf);
			return _getdns_list_find_and_add(
			    d->i.data.list, next, item);
		}
		d->i.dtype = t_dict;
		d->i.data.dict = _getdns_dict_create_with_mf(&dict->mf);
		return _getdns_dict_find_and_add(d->i.data.dict, next, item);
	}
	if (*key != '/' || !(next = strchr(key + 1, '/'))) {
		switch (d->i.dtype) {
		case t_dict   : getdns_dict_destroy(d->i.data.dict); break;
		case t_list   : getdns_list_destroy(d->i.data.list); break;
		case t_bindata: _getdns_bindata_destroy(
				      &dict->mf, d->i.data.bindata); break;
		default       : break;
		}
		d->i.dtype = t_int;
		d->i.data.n = 33355555;
		*item = &d->i;
		return GETDNS_RETURN_GOOD;

	} else switch (d->i.dtype) {  /* Key was nested reference */
	case t_dict:return _getdns_dict_find_and_add(d->i.data.dict,next,item);
	case t_list:return _getdns_list_find_and_add(d->i.data.list,next,item);
	default    :/* Trying to dereference a non list or dict */
	            return GETDNS_RETURN_WRONG_TYPE_REQUESTED;
	}
}				/* getdns_dict_find_and_add */


/*---------------------------------------- getdns_dict_get_names
*/
getdns_return_t
getdns_dict_get_names(const getdns_dict *dict, getdns_list **answer)
{
	struct getdns_dict_item *item;

	if (!dict || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	*answer = getdns_list_create_with_extended_memory_functions(
		dict->mf.mf_arg, dict->mf.mf.ext.malloc,
		dict->mf.mf.ext.realloc, dict->mf.mf.ext.free);
	if (!*answer)
		return GETDNS_RETURN_NO_SUCH_DICT_NAME;

	RBTREE_FOR(item, struct getdns_dict_item *,
		(_getdns_rbtree_t *)&(dict->root)) {
		_getdns_list_append_string(*answer, item->node.key);
	}
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_get_names */

/*---------------------------------------- getdns_dict_get_data_type */
getdns_return_t
getdns_dict_get_data_type(
    const getdns_dict *dict, const char *name, getdns_data_type *answer)
{
	getdns_return_t r;
	getdns_item *item;

	if (!dict || !name || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find(dict, name, &item)))
		return r;

	*answer = item->dtype;
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_get_data_type */

/*---------------------------------------- getdns_dict_get_dict */
getdns_return_t
getdns_dict_get_dict(
    const getdns_dict *dict, const char *name, getdns_dict **answer)
{
	getdns_return_t r;
	getdns_item *item;

	if (!dict || !name || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find(dict, name, &item)))
		return r;

	if (item->dtype != t_dict)
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	*answer = item->data.dict;
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_get_dict */

/*---------------------------------------- getdns_dict_get_list */
getdns_return_t
getdns_dict_get_list(
    const getdns_dict *dict, const char *name, getdns_list **answer)
{
	getdns_return_t r;
	getdns_item *item;

	if (!dict || !name || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find(dict, name, &item)))
		return r;

	if (item->dtype != t_list)
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	*answer = item->data.list;
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_get_list */

/*---------------------------------------- getdns_dict_get_bindata */
getdns_return_t
getdns_dict_get_bindata(
    const getdns_dict *dict, const char *name, getdns_bindata **answer)
{
	getdns_return_t r;
	getdns_item *item;

	if (!dict || !name || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find(dict, name, &item)))
		return GETDNS_RETURN_NO_SUCH_DICT_NAME;

	if (item->dtype != t_bindata)
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	*answer = item->data.bindata;
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_get_bindata */

/*---------------------------------------- getdns_dict_get_int */
getdns_return_t
getdns_dict_get_int(
    const getdns_dict *dict, const char *name, uint32_t *answer)
{
	getdns_return_t r;
	getdns_item *item;

	if (!dict || !name || !answer)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find(dict, name, &item)))
		return r;

	if (item->dtype != t_int)
		return GETDNS_RETURN_WRONG_TYPE_REQUESTED;

	*answer = item->data.n;
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_get_int */

struct getdns_dict *
getdns_dict_create_with_extended_memory_functions(
	void *userarg,
	void *(*malloc)(void *userarg, size_t),
	void *(*realloc)(void *userarg, void *, size_t),
	void (*free)(void *userarg, void *))
{
	struct getdns_dict *dict;
	mf_union mf;

	if (!malloc || !realloc || !free)
		return NULL;

	mf.ext.malloc = malloc;
	dict = userarg == MF_PLAIN
		 ? (struct getdns_dict*)(*mf.pln.malloc)(
			   sizeof(struct getdns_dict))
		 : (struct getdns_dict*)(*mf.ext.malloc)(userarg,
			   sizeof(struct getdns_dict));
	if (!dict)
		return NULL;

	dict->mf.mf_arg         = userarg;
	dict->mf.mf.ext.malloc  = malloc;
	dict->mf.mf.ext.realloc = realloc;
	dict->mf.mf.ext.free    = free;

	_getdns_rbtree_init(&(dict->root),
		(int (*)(const void *, const void *)) strcmp);
	return dict;
}

struct getdns_dict *
getdns_dict_create_with_memory_functions(void *(*malloc)(size_t),
	void *(*realloc)(void *, size_t), void (*free)(void *))
{
	mf_union mf;
	mf.pln.malloc = malloc;
	mf.pln.realloc = realloc;
	mf.pln.free = free;
	return getdns_dict_create_with_extended_memory_functions(
		MF_PLAIN, mf.ext.malloc, mf.ext.realloc, mf.ext.free);
}

/*-------------------------- getdns_dict_create_with_context */
struct getdns_dict *
getdns_dict_create_with_context(const getdns_context *context)
{
	if (context)
		return getdns_dict_create_with_extended_memory_functions(
			context->mf.mf_arg, context->mf.mf.ext.malloc,
			context->mf.mf.ext.realloc, context->mf.mf.ext.free);
	else
		return getdns_dict_create_with_memory_functions(&malloc,
			&realloc, &free);
}			/* getdns_dict_create_with_context */

/*---------------------------------------- getdns_dict_create */
struct getdns_dict *
getdns_dict_create()
{
	return getdns_dict_create_with_context(NULL);
}					/* getdns_dict_create */

/*---------------------------------------- _getdns_dict_copy */
/**
 * private function used to make a copy of a dict structure,
 * the caller is responsible * for freeing storage allocated to returned value
 * @param srcdict the dictionary structure to copy
 * @param dstdict the copy destination
 * @return the address of the copy of the dictionary structure on success
 * @return NULL on error (out of memory, invalid srcdict)
 */
getdns_return_t
_getdns_dict_copy(const struct getdns_dict * srcdict,
	struct getdns_dict ** dstdict)
{
	struct getdns_dict_item *item;
	char *key;
	getdns_return_t retval;

	if (!dstdict)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!srcdict) {
		*dstdict = NULL;
		return GETDNS_RETURN_GOOD;
	}
	*dstdict = getdns_dict_create_with_extended_memory_functions(
		srcdict->mf.mf_arg,
		srcdict->mf.mf.ext.malloc,
		srcdict->mf.mf.ext.realloc,
		srcdict->mf.mf.ext.free);
	if (!*dstdict)
		return GETDNS_RETURN_GENERIC_ERROR;

	RBTREE_FOR(item, struct getdns_dict_item *,
		(struct _getdns_rbtree_t *)&(srcdict->root)) {
		key = (char *) item->node.key;
		switch (item->i.dtype) {
		case t_bindata:
			retval = getdns_dict_set_bindata(*dstdict, key,
				item->i.data.bindata);
			break;

		case t_dict:
			retval = getdns_dict_set_dict(*dstdict, key,
				item->i.data.dict);
			break;

		case t_int:
			retval = getdns_dict_set_int(*dstdict, key,
				item->i.data.n);
			break;

		case t_list:
			retval = getdns_dict_set_list(*dstdict, key,
				item->i.data.list);
			break;
		default:
			retval = GETDNS_RETURN_WRONG_TYPE_REQUESTED;
			break;
		}
		if (retval != GETDNS_RETURN_GOOD) {
			getdns_dict_destroy(*dstdict);;
			*dstdict = NULL;
			return retval;
		}
	}
	return GETDNS_RETURN_GOOD;
}				/* _getdns_dict_copy */


/*---------------------------------------- getdns_dict_destroy */
void
getdns_dict_destroy(struct getdns_dict *dict)
{
	if (!dict) return;
	_getdns_traverse_postorder(&(dict->root), getdns_dict_item_free, dict);
	GETDNS_FREE(dict->mf, dict);
}				/* getdns_dict_destroy */

/*---------------------------------------- getdns_dict_set_dict */
getdns_return_t
_getdns_dict_set_this_dict(
    getdns_dict *dict, const char *name, getdns_dict *child_dict)
{
	getdns_item    *item;
	getdns_return_t r;

	if (!dict || !name || !child_dict)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find_and_add(dict, name, &item)))
		return r;

	item->dtype = t_dict;
	item->data.dict = child_dict;
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_set_dict */

getdns_return_t
getdns_dict_set_dict(
    getdns_dict *dict, const char *name, const getdns_dict *child_dict)
{
	getdns_dict    *newdict;
	getdns_return_t r;

	if ((r = _getdns_dict_copy(child_dict, &newdict)))
		return r;

	if ((r = _getdns_dict_set_this_dict(dict, name, newdict)))
		getdns_dict_destroy(newdict);

	return r;
}


/*---------------------------------------- getdns_dict_set_list */
getdns_return_t
_getdns_dict_set_this_list(
    getdns_dict *dict, const char *name, getdns_list *child_list)
{
	getdns_item    *item;
	getdns_return_t r;

	if (!dict || !name || !child_list)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find_and_add(dict, name, &item)))
		return r;

	item->dtype = t_list;
	item->data.list = child_list;
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_set_list */

getdns_return_t
getdns_dict_set_list(
    getdns_dict *dict, const char *name, const getdns_list *child_list)
{
	getdns_list    *newlist;
	getdns_return_t r;

	if ((r = _getdns_list_copy(child_list, &newlist)))
		return r;

	if ((r = _getdns_dict_set_this_list(dict, name, newlist)))
		getdns_list_destroy(newlist);

	return r;
}


/*---------------------------------------- getdns_dict_set_bindata */
getdns_return_t
_getdns_dict_set_this_bindata(
    getdns_dict *dict, const char *name, getdns_bindata *bindata)
{
	getdns_item    *item;
	getdns_return_t r;

	if (!dict || !name || !bindata)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find_and_add(dict, name, &item)))
		return r;

	item->dtype = t_bindata;
	item->data.bindata = bindata;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
_getdns_dict_set_const_bindata(
    getdns_dict *dict, const char *name, size_t size, const void *data)
{
	getdns_item    *item;
	getdns_bindata *newbindata;
	getdns_return_t r;

	if (!dict || !name)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!(newbindata = _getdns_bindata_copy(&dict->mf, size, data)))
		return GETDNS_RETURN_MEMORY_ERROR;

	if ((r = _getdns_dict_find_and_add(dict, name, &item))) {
		_getdns_bindata_destroy(&dict->mf, newbindata);
		return r;
	}
	item->dtype = t_bindata;
	item->data.bindata = newbindata;
	return GETDNS_RETURN_GOOD;
}

getdns_return_t
getdns_dict_set_bindata(
    getdns_dict *dict, const char *name, const getdns_bindata *child_bindata)
{
	return !child_bindata ? GETDNS_RETURN_INVALID_PARAMETER
	    : _getdns_dict_set_const_bindata(
	    dict, name, child_bindata->size, child_bindata->data);
}

/*---------------------------------------- getdns_dict_set_bindata */
getdns_return_t
getdns_dict_util_set_string(getdns_dict *dict,
    const char *name, const char *value)
{
	getdns_item    *item;
	getdns_bindata *newbindata;
	getdns_return_t r;

	if (!dict || !name || !value)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if (!(newbindata = _getdns_bindata_copy(
	    &dict->mf, strlen(value) + 1, (uint8_t *)value)))
		return GETDNS_RETURN_MEMORY_ERROR;

	newbindata->size -= 1;

	if ((r = _getdns_dict_find_and_add(dict, name, &item))) {
		_getdns_bindata_destroy(&dict->mf, newbindata);
		return r;
	}
	item->dtype = t_bindata;
	item->data.bindata = newbindata;
	return GETDNS_RETURN_GOOD;
}				/* getdns_dict_util_set_dict */

/*---------------------------------------- getdns_dict_set_int */
getdns_return_t
getdns_dict_set_int(getdns_dict *dict, const char *name, uint32_t child_uint32)
{
	getdns_item    *item;
	getdns_return_t r;

	if (!dict || !name)
		return GETDNS_RETURN_INVALID_PARAMETER;

	if ((r = _getdns_dict_find_and_add(dict, name, &item)))
		return r;

	item->dtype = t_int;
	item->data.n = child_uint32;
	return  GETDNS_RETURN_GOOD;
}				/* getdns_dict_set_int */

/*---------------------------------------- getdns_pp_dict */
/**
 * private function to help with indenting.
 * @param indent number of spaces to return
 * @return       a character string containing min(80, indent) spaces
 */
static const char *
getdns_indent(size_t indent)
{
	static const char *spaces = "                                        "
		"                                        ";
	return spaces + 80 - (indent < 80 ? indent : 0);
}				/* getdns_indent */

int
_getdns_bindata_is_dname(getdns_bindata *bindata)
{
	size_t i = 0, n_labels = 0;

	while (i < bindata->size && bindata->data[i]) {
		if (bindata->data[i] & 0xC0) /* Compression pointer! */
			return 0;

		i += ((size_t)bindata->data[i]) + 1;
		n_labels++;
	}
	if (i < bindata->size && !bindata->data[i]) {
		n_labels++;
		i++;
	}

	return i == bindata->size && n_labels > 1 &&
		bindata->data[bindata->size - 1] == 0;
}

static int
getdns_pp_base64(gldns_buffer *buf, getdns_bindata *bindata)
{
	size_t p = gldns_buffer_position(buf);
	size_t base64str_sz;

	if (gldns_buffer_printf(buf, " <bindata of ") < 0)
		return -1;

	base64str_sz = gldns_b64_ntop_calculate_size(bindata->size);
	if (!gldns_buffer_reserve(buf, base64str_sz))
		return -1;

	gldns_buffer_skip(buf, gldns_b64_ntop(bindata->data, bindata->size,
	    (char *)gldns_buffer_current(buf), base64str_sz));

	if (gldns_buffer_printf(buf, ">") < 0)
		return -1;

	return gldns_buffer_position(buf) - p;
}

/*---------------------------------------- getdns_pp_bindata */
/**
 * private function to pretty print bindata to a gldns_buffer
 * @param buf     buffer to write to
 * @param bindata the bindata to print
 * @return        on success the number of written characters
 *                if an output error is encountered, a negative value
 */
static int
getdns_pp_bindata(gldns_buffer *buf, getdns_bindata *bindata,
    int rdata_raw, int json)
{
	size_t i, p = gldns_buffer_position(buf);
	uint8_t *dptr;
	char spc[1024];

	if (!json && gldns_buffer_printf(buf, " <bindata ") < 0)
		return -1;

	/* Walk through all printable characters */
	i = 0;
	if (!rdata_raw)
		while (i < bindata->size && isprint(bindata->data[i]))
			i++;

	if (bindata->size > 0 && i == bindata->size) {     /* all printable? */

		if (json) {
			const uint8_t *s = bindata->data;
			const uint8_t *e = s + bindata->size;
			const uint8_t *b;

			if (!gldns_buffer_reserve(buf, (e - s) + 2))
				return -1;
			gldns_buffer_write_u8(buf, '"');
			for (;;) {
				for ( b = s
				    ; b < e && *b != '\\' && *b != '"'
				    ; b++)
					; /* pass */
				if (b == e)
					break;
				if (!gldns_buffer_reserve(buf, (b - s) + 3))
					return -1;
				gldns_buffer_write(buf, s, b - s);
				gldns_buffer_write_u8(buf, '\\');
				gldns_buffer_write_u8(buf, *b);
				s = b + 1;
			}
			if (s < e)
			       	gldns_buffer_write(buf, s, e - s);
			gldns_buffer_write_u8(buf, '"');
		} else {
			(void)snprintf(spc, sizeof(spc), "of \"%%.%ds\"%s>",
			    (int)(i > 32 ? 32 : i), (i > 32 ? "..." : ""));
			if (gldns_buffer_printf(buf, spc, bindata->data) < 0)
				return -1;
		}

	} else if (bindata->size > 1 &&         /* null terminated printable */
	    i == bindata->size - 1 && bindata->data[i] == 0) {

		if (gldns_buffer_printf(
		    buf, (json ? "\"%s\"" : "of \"%s\">"), bindata->data) < 0)
			return -1;

	} else if (bindata->size == 1 && *bindata->data == 0) {
		if (gldns_buffer_printf(buf, json ? "\".\"" : "for .>") < 0)
			return -1;

	} else if (_getdns_bindata_is_dname(bindata)) {
		(void)gldns_wire2str_dname_buf(
		    bindata->data, bindata->size, spc, sizeof(spc));
		if (gldns_buffer_printf(
		    buf, (json ? "\"%s\"" : "for %s>"), spc) < 0)
			return -1;
	} else if (json) {
		if (gldns_buffer_printf(buf, "[") < 0)
			return -1;
		for (dptr = bindata->data;
			dptr < bindata->data + bindata->size; dptr++) {
			if (dptr > bindata->data) {
				if (gldns_buffer_printf(buf, ",") < 0)
					return -1;
			}
			if (gldns_buffer_printf(buf, "%d", (int)*dptr) < 0)
				return -1;
		}
		if (gldns_buffer_printf(buf, "]") < 0)
			return -1;
	} else {
		if (gldns_buffer_printf(buf, "of 0x") < 0)
			return -1;
		for (dptr = bindata->data;
			dptr < bindata->data + bindata->size; dptr++) {
			if (dptr - bindata->data >= 16) {
				if (gldns_buffer_printf(buf, "...") < 0)
					return -1;
				break;
			}
			if (gldns_buffer_printf(buf, "%.2x", *dptr) < 0)
				return -1;
		}
		if (gldns_buffer_printf(buf, ">") < 0)
			return -1;
	}
	return gldns_buffer_position(buf) - p;
}				/* getdns_pp_bindata */

static int
getdns_pp_dict(gldns_buffer * buf, size_t indent,
	const getdns_dict *dict, int json);

static const char *unknown_str_l[] = {" <unknown>", " null", "null"};

/*---------------------------------------- getdns_pp_list */
/**
 * private function to pretty print list to a gldns_buffer
 * @param buf    buffer to write to
 * @param indent number of spaces to append after newline
 * @param list   the to list print
 * @param for_literals The list is a list of literals.
 *               Show the literal instead of the value.
 * @return       on success the number of written characters
 *               if an output error is encountered, a negative value
 */
static int
getdns_pp_list(gldns_buffer *buf, size_t indent, const getdns_list *list,
    int for_literals, int json)
{
	size_t i, length, p = gldns_buffer_position(buf);
	getdns_data_type dtype;
	struct getdns_dict *dict_item;
	struct getdns_list *list_item;
	struct getdns_bindata *bindata_item;
	uint32_t int_item;
	const char *strval;

	if (list == NULL)
		return 0;

	if (gldns_buffer_printf(buf, "[") < 0)
		return -1;

	if (getdns_list_get_length(list, &length) != GETDNS_RETURN_GOOD)
		return -1;

	indent += 2;
	for (i = 0; i < length; i++) {
		if (i && gldns_buffer_printf(buf, ",") < 0)
			return -1;
		if (json < 2 && gldns_buffer_printf(buf, "\n%s",
		    getdns_indent(indent)) < 0)
			return -1;

		if (getdns_list_get_data_type(list, i, &dtype) !=
		    GETDNS_RETURN_GOOD)
			return -1;

		switch (dtype) {
		case t_int:

			if (getdns_list_get_int(list, i, &int_item))
				return -1;
			if (!json && for_literals &&
			    (strval =
			     _getdns_get_const_info(int_item)->name)) {
				if (gldns_buffer_printf(buf, "%s", strval) < 0)
					return -1;
			} else if (0 > 
			    gldns_buffer_printf(buf, "%d", (int)int_item))
				return -1;
			break;

		case t_bindata:
			if (getdns_list_get_bindata(list, i, &bindata_item) !=
			    GETDNS_RETURN_GOOD)
				return -1;
			if (getdns_pp_bindata(
			    buf, bindata_item, 0, json) < 0)
				return -1;
			break;

		case t_list:
			if (getdns_list_get_list(list, i, &list_item) !=
			    GETDNS_RETURN_GOOD)
				return -1;
			if (getdns_pp_list(
			    buf, indent, list_item, 0, json) < 0)
				return -1;
			break;

		case t_dict:
			if (getdns_list_get_dict(list, i, &dict_item) !=
				GETDNS_RETURN_GOOD)
				return -1;
			if (getdns_pp_dict(buf, indent, dict_item, json) < 0)
				return -1;
			break;

		default:
			if (gldns_buffer_printf(
			    buf, "%s", unknown_str_l[json]) < 0)
				return -1;
		}
	}
	indent -= 2;
	if (json < 2 && i && gldns_buffer_printf(
	    buf, "\n%s", getdns_indent(indent)) < 0)
		return -1;
	if (gldns_buffer_printf(buf, "]") < 0)
		return -1;

	return gldns_buffer_position(buf) - p;
}				/* getdns_pp_list */

static int
_getdns_print_class(gldns_buffer *buf, uint32_t klass)
{
	switch (klass) {
	case GETDNS_RRCLASS_IN:
		(void) gldns_buffer_printf(buf, " GETDNS_RRCLASS_IN");
		return 1;
	case GETDNS_RRCLASS_CH:
		(void) gldns_buffer_printf(buf, " GETDNS_RRCLASS_CH");
		return 1;
	case GETDNS_RRCLASS_HS:
		(void) gldns_buffer_printf(buf, " GETDNS_RRCLASS_HS");
		return 1;
	case GETDNS_RRCLASS_NONE:
		(void) gldns_buffer_printf(buf, " GETDNS_RRCLASS_NONE");
		return 1;
	case GETDNS_RRCLASS_ANY:
		(void) gldns_buffer_printf(buf, " GETDNS_RRCLASS_ANY");
		return 1;
	}
	return 0;
}

static int
_getdns_print_opcode(gldns_buffer *buf, uint32_t opcode)
{
	switch (opcode) {
	case GETDNS_OPCODE_QUERY:
		(void) gldns_buffer_printf(buf, " GETDNS_OPCODE_QUERY");
		return 1;
	case GETDNS_OPCODE_IQUERY:
		(void) gldns_buffer_printf(buf, " GETDNS_OPCODE_IQUERY");
		return 1;
	case GETDNS_OPCODE_STATUS:
		(void) gldns_buffer_printf(buf, " GETDNS_OPCODE_STATUS");
		return 1;
	case GETDNS_OPCODE_NOTIFY:
		(void) gldns_buffer_printf(buf, " GETDNS_OPCODE_NOTIFY");
		return 1;
	case GETDNS_OPCODE_UPDATE:
		(void) gldns_buffer_printf(buf, " GETDNS_OPCODE_UPDATE");
		return 1;
	}
	return 0;
}

static int
_getdns_print_rcode(gldns_buffer *buf, uint32_t rcode)
{
	static const char *rcodes[] = {
		" GETDNS_RCODE_NOERROR" , " GETDNS_RCODE_FORMERR" ,
		" GETDNS_RCODE_SERVFAIL", " GETDNS_RCODE_NXDOMAIN",
		" GETDNS_RCODE_NOTIMP"  , " GETDNS_RCODE_REFUSED" ,
		" GETDNS_RCODE_YXDOMAIN", " GETDNS_RCODE_YXRRSET" ,
		" GETDNS_RCODE_NXRRSET" , " GETDNS_RCODE_NOTAUTH" ,
		" GETDNS_RCODE_NOTZONE" ,
		" GETDNS_RCODE_BADSIG"  , " GETDNS_RCODE_BADKEY"  ,
		" GETDNS_RCODE_BADTIME" , " GETDNS_RCODE_BADMODE" ,
		" GETDNS_RCODE_BADNAME" , " GETDNS_RCODE_BADALG"  ,
		" GETDNS_RCODE_BADTRUNC"
	};
	if (rcode <= 10)
		(void) gldns_buffer_printf(buf, "%s", rcodes[rcode]);
	else if (rcode >= 16 && rcode <= 22)
		(void) gldns_buffer_printf(buf, "%s", rcodes[rcode-6]);
	else
		return 0;
	return 1;
}

/*---------------------------------------- getdns_pp_dict */
/**
 * private function to pretty print dict to a gldns_buffer
 * @param buf    buffer to write to
 * @param indent number of spaces to append after newline
 * @param dict   the dict to print
 * @return       on success the number of written characters
 *               if an output error is encountered, a negative value
 */
static int
getdns_pp_dict(gldns_buffer * buf, size_t indent,
	const getdns_dict *dict, int json)
{
	size_t i, length, p = gldns_buffer_position(buf);
	struct getdns_dict_item *item;
	const char *strval;

	char abuf[80];

	if (dict == NULL)
		return 0;

	if (gldns_buffer_printf(buf, "{") < 0)
		return -1;

	i = 0;
	indent += 2;
	RBTREE_FOR(item, struct getdns_dict_item *,
		(_getdns_rbtree_t *)&(dict->root)) {

		if (i && gldns_buffer_printf(buf, ",") < 0)
			return -1;
		if (json < 2 && gldns_buffer_printf(
		    buf, "\n%s", getdns_indent(indent)) < 0)
			return -1;
		if (gldns_buffer_printf(
		    buf, "\"%s\":",(const char *)item->node.key) < 0)
			return -1;

		switch (item->i.dtype) {
		case t_int:
			if (!json &&
			    (strcmp(item->node.key, "type") == 0  ||
			     strcmp(item->node.key, "type_covered") == 0 ||
				 strcmp(item->node.key, "query_type") == 0 || 
			     strcmp(item->node.key, "qtype") == 0) &&
			    (strval = _getdns_rr_type_name(item->i.data.n))) {
				if (gldns_buffer_printf(
					buf, " GETDNS_RRTYPE_%s", strval) < 0)
					return -1;
				break;
			}
			if (!json &&
			    (strcmp(item->node.key, "answer_type") == 0  ||
			     strcmp(item->node.key, "dnssec_status") == 0 ||
			     strcmp(item->node.key, "tsig_status") == 0 ||
			     strcmp(item->node.key, "status") == 0 ||
			     strcmp(item->node.key, "append_name") == 0 ||
			     strcmp(item->node.key, "follow_redirects") == 0 ||
			     strcmp(item->node.key, "transport") == 0 ||
			     strcmp(item->node.key, "resolution_type") == 0 ||
			     strcmp(item->node.key, "tls_authentication") == 0 ||
			     strcmp(item->node.key, "tls_min_version") == 0 ||
			     strcmp(item->node.key, "tls_max_version") == 0 ||

			     /* extensions */
			     strcmp(item->node.key, "add_warning_for_bad_dns") == 0 ||
			     strcmp(item->node.key, "dnssec") == 0 ||
			     strcmp(item->node.key, "dnssec_return_all_statuses") == 0 ||
			     strcmp(item->node.key, "dnssec_return_full_validation_chain") == 0 ||
			     strcmp(item->node.key, "dnssec_return_only_secure") == 0 ||
			     strcmp(item->node.key, "dnssec_return_status") == 0 ||
			     strcmp(item->node.key, "dnssec_return_validation_chain") == 0 ||
#if defined(DNSSEC_ROADBLOCK_AVOIDANCE) && defined(HAVE_LIBUNBOUND)
			     strcmp(item->node.key, "dnssec_roadblock_avoidance") == 0 ||
#endif
#ifdef EDNS_COOKIES
			     strcmp(item->node.key, "edns_cookies") == 0 ||
#endif
			     strcmp(item->node.key, "return_api_information") == 0 ||
			     strcmp(item->node.key, "return_both_v4_and_v6") == 0 ||
			     strcmp(item->node.key, "return_call_reporting") == 0
			     ) &&
			    (strval =
			     _getdns_get_const_info(item->i.data.n)->name)) {
				if (gldns_buffer_printf(buf, " %s", strval) < 0)
					return -1;
				break;
			}
			if (!json &&
			    (strcmp(item->node.key, "class")  == 0  ||
			     strcmp(item->node.key, "qclass") == 0) &&
			    _getdns_print_class(buf, item->i.data.n))
				break;
			if (!json && strcmp(item->node.key, "opcode") == 0 &&
			    _getdns_print_opcode(buf, item->i.data.n))
				break;
			if (!json && strcmp(item->node.key, "rcode") == 0 &&
			    _getdns_print_rcode(buf, item->i.data.n))
				break;
			if (gldns_buffer_printf(
			    buf,(json < 2 ? " %d" : "%d"), item->i.data.n) < 0)
				return -1;
			break;

		case t_bindata:
			if ((strcmp(item->node.key, "address_data") == 0 ||
			     strcmp(item->node.key, "ipv4_address") == 0 ||
			     strcmp(item->node.key, "ipv6_address") == 0 ||
			     strcmp(item->node.key, "answer_ipv4_address") == 0 ||
			     strcmp(item->node.key, "answer_ipv6_address") == 0) &&
			    (item->i.data.bindata->size == 4  ||
			     item->i.data.bindata->size == 16 )) {

				if (gldns_buffer_printf(buf, 
				    (json ? "\"%s\"" : " <bindata for %s>"),
				    inet_ntop(( item->i.data.bindata->size == 4
				              ? AF_INET : AF_INET6)
				              , item->i.data.bindata->data
				              , abuf
				              , 40
				              )) < 0)
					return -1;
	
			} else if (!json &&
			    (strcmp(item->node.key, "pin-sha256") == 0 ||
			     strcmp(item->node.key, "value") == 0) &&
			     item->i.data.bindata->size > 0 &&
			     item->i.data.bindata->size % 4 == 0) {

				if (getdns_pp_base64(buf,
				    item->i.data.bindata) < 0)
					return -1;

			} else if (getdns_pp_bindata(
			    buf, item->i.data.bindata,
			    (strcmp(item->node.key, "rdata_raw") == 0),
			    json) < 0)
				return -1;
			break;

		case t_list:	/* Don't put empty lists on a new line */

			if (getdns_list_get_length(item->i.data.list, &length) !=
			    GETDNS_RETURN_GOOD)
				return -1;
			if (length == 0) {
				if (gldns_buffer_printf(
				    buf, (json < 2 ? " []" : "[]")) < 0)
					return -1;
				break;
			}
			if (json < 2 && gldns_buffer_printf(buf, "\n%s",
			    getdns_indent(indent)) < 0)
				return -1;
			if (getdns_pp_list(buf, indent, item->i.data.list, 
			    (strcmp(item->node.key, "namespaces") == 0 ||
			     strcmp(item->node.key, "dns_transport_list") == 0
			     || strcmp(item->node.key, "bad_dns") == 0),
			    json) < 0)
				return -1;
			break;

		case t_dict:
			if (json < 2 && gldns_buffer_printf(buf, "\n%s",
			    getdns_indent(indent)) < 0)
				return -1;
			if (getdns_pp_dict(
			    buf, indent, item->i.data.dict, json) < 0)
				return -1;
			break;

		default:
			if (gldns_buffer_printf(
			    buf, "%s", unknown_str_l[json]) < 0)
				return -1;
		}
		i++;
	}
	indent -= 2;
	if (json < 2 && i  && gldns_buffer_printf(
	    buf, "\n%s", getdns_indent(indent)) < 0)
		return -1;
	if (gldns_buffer_printf(buf, "}") < 0)
		return -1;

	return gldns_buffer_position(buf) - p;
}				/* getdns_pp_dict */

/*---------------------------------------- getdns_pretty_print_dict */
/**
 * Return a character string containing a "human readable" representation
 * of dict.
 * @param dict   the dict to pretty print
 * @return       the "human readable" representation of dict
 *               or NULL on error
 */
char *
getdns_pretty_print_dict(const struct getdns_dict *dict)
{
	gldns_buffer *buf;
	char *ret;

	if (!dict)
		return NULL;

	buf = gldns_buffer_new(8192);
	if (!buf)
		return NULL;

	if (getdns_pp_dict(buf, 0, dict, 0) < 0) {
		gldns_buffer_free(buf);
		return NULL;
	}
	ret = (char *) gldns_buffer_export(buf);
	gldns_buffer_free(buf);
	return ret;
}				/* getdns_pretty_print_dict */

int
getdns_pretty_snprint_dict(char *str, size_t size, const getdns_dict *dict)
{
	gldns_buffer buf;

	if (!dict) return -1;

	gldns_buffer_init_vfixed_frm_data(&buf, str, size);
	return getdns_pp_dict(&buf, 0, dict, 0) < 0
	     ? -1 : (int)gldns_buffer_position(&buf);
}

char *
getdns_pretty_print_list(const getdns_list *list)
{
	gldns_buffer *buf;
	char *ret;

	if (!list)
		return NULL;

	buf = gldns_buffer_new(4096);
	if (!buf)
		return NULL;

	if (getdns_pp_list(buf, 0, list, 0, 0) < 0) {
		gldns_buffer_free(buf);
		return NULL;
	}
	ret = (char *) gldns_buffer_export(buf);
	gldns_buffer_free(buf);
	return ret;
}

int
getdns_pretty_snprint_list(char *str, size_t size, const getdns_list *list)
{
	gldns_buffer buf;

	if (!list) return -1;

	gldns_buffer_init_vfixed_frm_data(&buf, str, size);
	return getdns_pp_list(&buf, 0, list, 0, 0) < 0
	     ? -1 : (int)gldns_buffer_position(&buf);
}

char *
getdns_print_json_dict(const getdns_dict *dict, int pretty)
{
	gldns_buffer *buf;
	char *ret;

	if (!dict)
		return NULL;

	buf = gldns_buffer_new(8192);
	if (!buf)
		return NULL;

	if (getdns_pp_dict(buf, 0, dict, pretty ? 1 : 2) < 0) {
		gldns_buffer_free(buf);
		return NULL;
	}
	ret = (char *) gldns_buffer_export(buf);
	gldns_buffer_free(buf);
	return ret;
}				/* getdns_print_json_dict */

int
getdns_snprint_json_dict(
    char *str, size_t size, const getdns_dict *dict, int pretty)
{
	gldns_buffer buf;

	if (!dict) return -1;

	gldns_buffer_init_vfixed_frm_data(&buf, str, size);
	return getdns_pp_dict(&buf, 0, dict, pretty ? 1 : 2) < 0
	     ? -1 : (int)gldns_buffer_position(&buf);
}

char *
getdns_print_json_list(const getdns_list *list, int pretty)
{
	gldns_buffer *buf;
	char *ret;

	if (!list)
		return NULL;

	buf = gldns_buffer_new(4096);
	if (!buf)
		return NULL;

	if (getdns_pp_list(buf, 0, list, 0, pretty ? 1 : 2) < 0) {
		gldns_buffer_free(buf);
		return NULL;
	}
	ret = (char *) gldns_buffer_export(buf);
	gldns_buffer_free(buf);
	return ret;
}

int
getdns_snprint_json_list(
    char *str, size_t size, const getdns_list *list, int pretty)
{
	gldns_buffer buf;

	if (!list) return -1;

	gldns_buffer_init_vfixed_frm_data(&buf, str, size);
	return getdns_pp_list(&buf, 0, list, 0, pretty ? 1 : 2) < 0
	     ? -1 : (int)gldns_buffer_position(&buf);
}

/* dict.c */
