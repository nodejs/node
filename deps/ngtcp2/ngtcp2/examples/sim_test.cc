/*
 * ngtcp2
 *
 * Copyright (c) 2025 ngtcp2 contributors
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
#include "sim_test.h"

#include <iostream>

#include "sim.h"
#include "util.h"

using namespace std::literals;

namespace ngtcp2 {

namespace {
const MunitTest tests[]{
  munit_void_test(test_sim_handshake),
  munit_void_test(test_sim_unistream),
  munit_test_end(),
};
} // namespace

const MunitSuite sim_suite{
  .prefix = "/sim",
  .tests = tests,
};

void test_sim_handshake(void) {
  struct Test {
    const char *name;
    Timestamp::duration delay;
  };

  auto tests = std::to_array<Test>({
    {
      .name = "short delay",
      .delay = 15ms,
    },
    {
      .name = "long delay",
      .delay = 1h,
    },
  });

  for (auto &t : tests) {
    munit_logf(MUNIT_LOG_INFO, "testcase: %s", t.name);

    auto link = LinkConfig{
      .delay = t.delay,
    };

    HandshakeApp clapp;
    auto cl = default_client_endpoint_config();
    clapp.configure(cl);
    cl.link = link;

    HandshakeApp svapp;
    auto sv = default_server_endpoint_config();
    svapp.configure(sv);
    sv.link = link;

    int rv;

    {
      rv = Simulator{Endpoint(cl), Endpoint(sv)}.run();
    }

    assert_int(0, ==, rv);
    assert_true(clapp.get_handshake_confirmed());
    assert_true(svapp.get_handshake_confirmed());
  }
}

void test_sim_unistream(void) {
  struct Test {
    const char *name;
    double loss;
  };

  auto tests = std::to_array<Test>({
    {
      .name = "no loss",
    },
    {
      .name = "1% loss",
      .loss = 0.01,
    },
  });

  for (auto &t : tests) {
    munit_logf(MUNIT_LOG_INFO, "testcase: %s", t.name);

    auto link = LinkConfig{
      .delay = 15ms,
      .rate = 10_mbps,
      .limit = MAX_UDP_PAYLOAD_SIZE * 25,
      .loss = t.loss,
      .seed = munit_rand_uint32(),
    };

    HandshakeApp clapp;
    auto cl = default_client_endpoint_config();
    clapp.configure(cl);
    cl.params.initial_max_streams_uni = 1;
    cl.params.initial_max_stream_data_uni = 6_m;
    cl.params.initial_max_data = 6_m;
    cl.link = link;

    UniStreamApp svapp(10_m);
    auto sv = default_server_endpoint_config();
    svapp.configure(sv);
    sv.link = link;

    int rv;

    {
      rv = Simulator{Endpoint(cl), Endpoint(sv)}.run();
    }

    assert_int(0, ==, rv);
    assert_true(svapp.is_all_bytes_sent());
    assert_uint64(link.compute_expected_goodput(link.delay * 2), <=,
                  svapp.compute_goodput());
  }
}

} // namespace ngtcp2
