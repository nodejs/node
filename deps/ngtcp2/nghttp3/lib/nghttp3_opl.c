/*
 * nghttp3
 *
 * Copyright (c) 2022 nghttp3 contributors
 * Copyright (c) 2022 ngtcp2 contributors
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
#include "nghttp3_opl.h"

void nghttp3_opl_init(nghttp3_opl *opl) { opl->head = NULL; }

void nghttp3_opl_push(nghttp3_opl *opl, nghttp3_opl_entry *ent) {
  ent->next = opl->head;
  opl->head = ent;
}

nghttp3_opl_entry *nghttp3_opl_pop(nghttp3_opl *opl) {
  nghttp3_opl_entry *ent = opl->head;

  if (!ent) {
    return NULL;
  }

  opl->head = ent->next;

  return ent;
}

void nghttp3_opl_clear(nghttp3_opl *opl) { opl->head = NULL; }
