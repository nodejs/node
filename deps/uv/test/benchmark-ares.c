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

#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strlen */

static uv_loop_t* loop;

static ares_channel channel;
static struct ares_options options;
static int optmask;

static int ares_callbacks;
static int ares_errors;
static int argument;

#define NUM_CALLS_TO_START    1000

static int64_t start_time;
static int64_t end_time;


/* callback method. */
static void aresbynamecallback(void *arg,
                               int status,
                               int timeouts,
                               struct hostent *hostent) {
  ares_callbacks++;
  if (status != 0) {
    ares_errors++;
  }
}


static void prep_tcploopback()
{
  /* for test, use echo server - TCP port TEST_PORT on loopback */
  struct sockaddr_in test_server = uv_ip4_addr("127.0.0.1", 0);
  int rc = 0;
  optmask = 0;

  optmask = ARES_OPT_SERVERS | ARES_OPT_TCP_PORT | ARES_OPT_FLAGS;
  options.servers = &test_server.sin_addr;
  options.nservers = 1;
  options.tcp_port = TEST_PORT_2;
  options.flags = ARES_FLAG_USEVC;

  rc = uv_ares_init_options(loop, &channel, &options, optmask);

  ASSERT(rc == ARES_SUCCESS);
}


BENCHMARK_IMPL(gethostbyname) {

  int rc = 0;
  int ares_start;;

  rc = ares_library_init(ARES_LIB_INIT_ALL);
  if (rc != 0) {
    printf("ares library init fails %d\n", rc);
    return 1;
  }

  loop = uv_default_loop();

  ares_callbacks = 0;
  ares_errors = 0;

  start_time = uv_hrtime();

  prep_tcploopback();

  for (ares_start = 0; ares_start < NUM_CALLS_TO_START; ares_start++) {
    ares_gethostbyname(channel,
                       "echos.srv",
                       AF_INET,
                       &aresbynamecallback,
                       &argument);
  }

  uv_run(loop);

  uv_ares_destroy(loop, channel);

  end_time = uv_hrtime();

  if (ares_errors > 0) {
    printf("There were %d failures\n", ares_errors);
  }
  LOGF("ares_gethostbyname: %.0f req/s\n",
       1e9 * ares_callbacks / (double)(end_time - start_time));

  return 0;
}
