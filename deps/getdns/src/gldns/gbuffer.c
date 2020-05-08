/*
 * buffer.c -- generic memory buffer .
 *
 * Copyright (c) 2001-2008, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */
/**
 * \file
 *
 * This file contains the definition of gldns_buffer, and functions to manipulate those.
 */
#include "config.h"
#include "gldns/gbuffer.h"
#include <stdarg.h>
#include <stdlib.h>

gldns_buffer *
gldns_buffer_new(size_t capacity)
{
	gldns_buffer *buffer = (gldns_buffer*)malloc(sizeof(gldns_buffer));

	if (!buffer) {
		return NULL;
	}
	
	buffer->_data = (uint8_t *) malloc(capacity);
	if (!buffer->_data) {
		free(buffer);
		return NULL;
	}
	
	buffer->_position = 0;
	buffer->_limit = buffer->_capacity = capacity;
	buffer->_fixed = 0;
	buffer->_vfixed = 0;
	buffer->_status_err = 0;
	
	gldns_buffer_invariant(buffer);
	
	return buffer;
}

void
gldns_buffer_new_frm_data(gldns_buffer *buffer, void *data, size_t size)
{
	assert(data != NULL);

	buffer->_position = 0; 
	buffer->_limit = buffer->_capacity = size;
	buffer->_fixed = 0;
	buffer->_vfixed = 0;
	if (!buffer->_fixed && buffer->_data)
		free(buffer->_data);
	buffer->_data = malloc(size);
	if(!buffer->_data) {
		buffer->_status_err = 1;
		return;
	}
	memcpy(buffer->_data, data, size);
	buffer->_status_err = 0;
	
	gldns_buffer_invariant(buffer);
}

void
gldns_buffer_init_frm_data(gldns_buffer *buffer, void *data, size_t size)
{
	memset(buffer, 0, sizeof(*buffer));
	buffer->_data = data;
	buffer->_capacity = buffer->_limit = size;
	buffer->_fixed = 1;
	buffer->_vfixed = 0;
}

void
gldns_buffer_init_vfixed_frm_data(gldns_buffer *buffer, void *data, size_t size)
{
	memset(buffer, 0, sizeof(*buffer));
	buffer->_data = data;
	buffer->_capacity = buffer->_limit = size;
	buffer->_fixed = 1;
	buffer->_vfixed = 1;
}

int
gldns_buffer_set_capacity(gldns_buffer *buffer, size_t capacity)
{
	void *data;
	
	gldns_buffer_invariant(buffer);
	assert(buffer->_position <= capacity && !buffer->_fixed);

	data = (uint8_t *) realloc(buffer->_data, capacity);
	if (!data) {
		buffer->_status_err = 1;
		return 0;
	} else {
		buffer->_data = data;
		buffer->_limit = buffer->_capacity = capacity;
		return 1;
	}
}

int
gldns_buffer_reserve(gldns_buffer *buffer, size_t amount)
{
	gldns_buffer_invariant(buffer);
	if (buffer->_vfixed)
		return 1;
	assert(!buffer->_fixed);
	if (buffer->_capacity < buffer->_position + amount) {
		size_t new_capacity = buffer->_capacity * 3 / 2;

		if (new_capacity < buffer->_position + amount) {
			new_capacity = buffer->_position + amount;
		}
		if (!gldns_buffer_set_capacity(buffer, new_capacity)) {
			buffer->_status_err = 1;
			return 0;
		}
	}
	buffer->_limit = buffer->_capacity;
	return 1;
}

int
gldns_buffer_printf(gldns_buffer *buffer, const char *format, ...)
{
	va_list args;
	int written = 0;
	size_t remaining;
	
	if (gldns_buffer_status_ok(buffer)) {
		gldns_buffer_invariant(buffer);
		assert(buffer->_limit == buffer->_capacity);

		remaining = gldns_buffer_remaining(buffer);
		va_start(args, format);
		written = vsnprintf((char *) gldns_buffer_current(buffer), remaining,
				    format, args);
		va_end(args);
		if (written == -1) {
			buffer->_status_err = 1;
			return -1;
		} else if (!buffer->_vfixed && (size_t) written >= remaining) {
			if (!gldns_buffer_reserve(buffer, (size_t) written + 1)) {
				buffer->_status_err = 1;
				return -1;
			}
			va_start(args, format);
			written = vsnprintf((char *) gldns_buffer_current(buffer),
			    gldns_buffer_remaining(buffer), format, args);
			va_end(args);
			if (written == -1) {
				buffer->_status_err = 1;
				return -1;
			}
		}
		buffer->_position += written;
	}
	return written;
}

void
gldns_buffer_free(gldns_buffer *buffer)
{
	if (!buffer) {
		return;
	}

	if (!buffer->_fixed)
		free(buffer->_data);

	free(buffer);
}

void *
gldns_buffer_export(gldns_buffer *buffer)
{
	buffer->_fixed = 1;
	return buffer->_data;
}

void 
gldns_buffer_copy(gldns_buffer* result, gldns_buffer* from)
{
	size_t tocopy = gldns_buffer_limit(from);

	if(tocopy > gldns_buffer_capacity(result))
		tocopy = gldns_buffer_capacity(result);
	gldns_buffer_clear(result);
	gldns_buffer_write(result, gldns_buffer_begin(from), tocopy);
	gldns_buffer_flip(result);
}
