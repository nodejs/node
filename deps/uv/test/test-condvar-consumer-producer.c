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

#include "uv.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>

#define MAX_CONSUMERS 32
#define MAX_LOOPS 1000

struct buffer_s {
  ngx_queue_t queue;
  int data;
};
typedef struct buffer_s buffer_t;

static ngx_queue_t queue;
static uv_mutex_t mutex;
static uv_cond_t empty;
static uv_cond_t full;

static volatile int finished_consumers = 0;


static void produce(int value) {
  buffer_t* buf;

  buf = malloc(sizeof(*buf));
  ngx_queue_init(&buf->queue);
  buf->data = value;
  ngx_queue_insert_tail(&queue, &buf->queue);
}


static int consume(void) {
  ngx_queue_t* q;
  buffer_t* buf;
  int data;

  ASSERT(!ngx_queue_empty(&queue));
  q = ngx_queue_last(&queue);
  ngx_queue_remove(q);

  buf = ngx_queue_data(q, buffer_t, queue);
  data = buf->data;
  free(buf);

  return data;
}


static void producer(void* arg) {
  int i;

  (void) arg;

  for (i = 0; i < MAX_LOOPS * MAX_CONSUMERS; i++) {
    uv_mutex_lock(&mutex);
    while(!ngx_queue_empty(&queue))
      uv_cond_wait(&empty, &mutex);
    produce(i);
    uv_cond_signal(&full);
    uv_mutex_unlock(&mutex);
  }
}


static void consumer(void* arg) {
  int i;
  int value;

  (void) arg;

  for (i = 0; i < MAX_LOOPS; i++) {
    uv_mutex_lock(&mutex);
    while (ngx_queue_empty(&queue))
      uv_cond_wait(&full, &mutex);
    value = consume();
    ASSERT(value < MAX_LOOPS * MAX_CONSUMERS);
    uv_cond_signal(&empty);
    uv_mutex_unlock(&mutex);
  }

  finished_consumers++;
}


TEST_IMPL(consumer_producer) {
  int i;
  uv_thread_t cthreads[MAX_CONSUMERS];
  uv_thread_t pthread;

  ngx_queue_init(&queue);
  ASSERT(0 == uv_mutex_init(&mutex));
  ASSERT(0 == uv_cond_init(&empty));
  ASSERT(0 == uv_cond_init(&full));

  for (i = 0; i < MAX_CONSUMERS; i++) {
    ASSERT(0 == uv_thread_create(&cthreads[i], consumer, NULL));
  }

  ASSERT(0 == uv_thread_create(&pthread, producer, NULL));

  for (i = 0; i < MAX_CONSUMERS; i++) {
    ASSERT(0 == uv_thread_join(&cthreads[i]));
  }

  ASSERT(0 == uv_thread_join(&pthread));

  LOGF("finished_consumers: %d\n", finished_consumers);
  ASSERT(finished_consumers == MAX_CONSUMERS);

  uv_cond_destroy(&empty);
  uv_cond_destroy(&full);
  uv_mutex_destroy(&mutex);

  return 0;
}
