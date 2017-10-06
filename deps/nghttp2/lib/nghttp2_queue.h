/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGHTTP2_QUEUE_H
#define NGHTTP2_QUEUE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <nghttp2/nghttp2.h>

typedef struct nghttp2_queue_cell {
  void *data;
  struct nghttp2_queue_cell *next;
} nghttp2_queue_cell;

typedef struct { nghttp2_queue_cell *front, *back; } nghttp2_queue;

void nghttp2_queue_init(nghttp2_queue *queue);
void nghttp2_queue_free(nghttp2_queue *queue);
int nghttp2_queue_push(nghttp2_queue *queue, void *data);
void nghttp2_queue_pop(nghttp2_queue *queue);
void *nghttp2_queue_front(nghttp2_queue *queue);
void *nghttp2_queue_back(nghttp2_queue *queue);
int nghttp2_queue_empty(nghttp2_queue *queue);

#endif /* NGHTTP2_QUEUE_H */
