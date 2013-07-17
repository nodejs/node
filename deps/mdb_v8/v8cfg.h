/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * v8cfg.h: canned configurations for previous V8 versions
 */

#ifndef V8CFG_H
#define	V8CFG_H

#include <sys/types.h>
#include <sys/mdb_modapi.h>

typedef struct {
	const char 	*v8cs_name;	/* symbol name */
	intptr_t	v8cs_value;	/* symbol value */
} v8_cfg_symbol_t;

typedef struct v8_cfg {
	const char 	*v8cfg_name;	/* canned config name */
	const char 	*v8cfg_label;	/* description */
	v8_cfg_symbol_t	*v8cfg_symbols;	/* actual symbol values */

	int (*v8cfg_iter)(struct v8_cfg *, int (*)(mdb_symbol_t *, void *),
	    void *);
	int (*v8cfg_readsym)(struct v8_cfg *, const char *, intptr_t *);
} v8_cfg_t;

extern v8_cfg_t v8_cfg_04;
extern v8_cfg_t v8_cfg_06;
extern v8_cfg_t v8_cfg_target;
extern v8_cfg_t *v8_cfgs[];

#endif /* V8CFG_H */
