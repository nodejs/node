/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
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
#include "debug.h"

#include <unistd.h>

#include <cassert>
#include <random>
#include <iostream>
#include <array>

#include "util.h"

using namespace std::literals;

namespace ngtcp2 {

namespace debug {

namespace {
auto randgen = util::make_mt19937();
} // namespace

namespace {
auto *outfile = stderr;
} // namespace

int handshake_completed(ngtcp2_conn *conn, void *user_data) {
  std::println(outfile, "QUIC handshake has completed");
  return 0;
}

int handshake_confirmed(ngtcp2_conn *conn, void *user_data) {
  std::println(outfile, "QUIC handshake has been confirmed");
  return 0;
}

bool packet_lost(double prob) {
  auto p = std::uniform_real_distribution<>(0, 1)(randgen);
  return p < prob;
}

void print_crypto_data(ngtcp2_encryption_level encryption_level,
                       std::span<const uint8_t> data) {
  const char *encryption_level_str;
  switch (encryption_level) {
  case NGTCP2_ENCRYPTION_LEVEL_INITIAL:
    encryption_level_str = "Initial";
    break;
  case NGTCP2_ENCRYPTION_LEVEL_HANDSHAKE:
    encryption_level_str = "Handshake";
    break;
  case NGTCP2_ENCRYPTION_LEVEL_1RTT:
    encryption_level_str = "1-RTT";
    break;
  default:
    assert(0);
    abort();
  }
  std::println(outfile, "Ordered CRYPTO data in {} crypto level",
               encryption_level_str);
  util::hexdump(outfile, data);
}

void print_stream_data(int64_t stream_id, std::span<const uint8_t> data) {
  std::println(outfile, "Ordered STREAM data stream_id={:#x}", stream_id);
  util::hexdump(outfile, data);
}

void print_initial_secret(std::span<const uint8_t> data) {
  std::println(outfile, "initial_secret={}", util::format_hex(data));
}

void print_client_in_secret(std::span<const uint8_t> data) {
  std::println(outfile, "client_in_secret={}", util::format_hex(data));
}

void print_server_in_secret(std::span<const uint8_t> data) {
  std::println(outfile, "server_in_secret={}", util::format_hex(data));
}

void print_handshake_secret(std::span<const uint8_t> data) {
  std::println(outfile, "handshake_secret={}", util::format_hex(data));
}

void print_client_hs_secret(std::span<const uint8_t> data) {
  std::println(outfile, "client_hs_secret={}", util::format_hex(data));
}

void print_server_hs_secret(std::span<const uint8_t> data) {
  std::println(outfile, "server_hs_secret={}", util::format_hex(data));
}

void print_client_0rtt_secret(std::span<const uint8_t> data) {
  std::println(outfile, "client_0rtt_secret={}", util::format_hex(data));
}

void print_client_1rtt_secret(std::span<const uint8_t> data) {
  std::println(outfile, "client_1rtt_secret={}", util::format_hex(data));
}

void print_server_1rtt_secret(std::span<const uint8_t> data) {
  std::println(outfile, "server_1rtt_secret={}", util::format_hex(data));
}

void print_client_pp_key(std::span<const uint8_t> data) {
  std::println(outfile, "+ client_pp_key={}", util::format_hex(data));
}

void print_server_pp_key(std::span<const uint8_t> data) {
  std::println(outfile, "+ server_pp_key={}", util::format_hex(data));
}

void print_client_pp_iv(std::span<const uint8_t> data) {
  std::println(outfile, "+ client_pp_iv={}", util::format_hex(data));
}

void print_server_pp_iv(std::span<const uint8_t> data) {
  std::println(outfile, "+ server_pp_iv={}", util::format_hex(data));
}

void print_client_pp_hp(std::span<const uint8_t> data) {
  std::println(outfile, "+ client_pp_hp={}", util::format_hex(data));
}

void print_server_pp_hp(std::span<const uint8_t> data) {
  std::println(outfile, "+ server_pp_hp={}", util::format_hex(data));
}

void print_secrets(std::span<const uint8_t> secret,
                   std::span<const uint8_t> key, std::span<const uint8_t> iv,
                   std::span<const uint8_t> hp) {
  std::println(stderr, R"(+ secret={}
+ key={}
+ iv={}
+ hp={})",
               util::format_hex(secret), util::format_hex(key),
               util::format_hex(iv), util::format_hex(hp));
}

void print_secrets(std::span<const uint8_t> secret,
                   std::span<const uint8_t> key, std::span<const uint8_t> iv) {
  std::println(stderr, R"(+ secret={}
+ key={}
+ iv={})",
               util::format_hex(secret), util::format_hex(key),
               util::format_hex(iv));
}

void print_hp_mask(std::span<const uint8_t> mask,
                   std::span<const uint8_t> sample) {
  std::println(outfile, "mask={} sample={}", util::format_hex(mask),
               util::format_hex(sample));
}

void log_printf(void *user_data, const char *fmt, ...) {
  va_list ap;
  std::array<char, 4096> buf;

  va_start(ap, fmt);
  auto n = vsnprintf(buf.data(), buf.size(), fmt, ap);
  va_end(ap);

  if (static_cast<size_t>(n) >= buf.size()) {
    n = buf.size() - 1;
  }

  buf[static_cast<size_t>(n++)] = '\n';

  while (write(fileno(stderr), buf.data(), static_cast<size_t>(n)) == -1 &&
         errno == EINTR)
    ;
}

void path_validation(const ngtcp2_path *path,
                     ngtcp2_path_validation_result res) {
  auto local_addr = util::straddr(
    reinterpret_cast<sockaddr *>(path->local.addr), path->local.addrlen);
  auto remote_addr = util::straddr(
    reinterpret_cast<sockaddr *>(path->remote.addr), path->remote.addrlen);

  std::println(
    stderr, "Path validation against path {{local:{}, remote:{}}} {}",
    local_addr, remote_addr,
    res == NGTCP2_PATH_VALIDATION_RESULT_SUCCESS ? "succeeded" : "failed");
}

void print_http_begin_request_headers(int64_t stream_id) {
  std::println(outfile, "http: stream {:#x} request headers started",
               stream_id);
}

void print_http_begin_response_headers(int64_t stream_id) {
  std::println(outfile, "http: stream {:#x} response headers started",
               stream_id);
}

namespace {
void print_header(std::span<const uint8_t> name, std::span<const uint8_t> value,
                  uint8_t flags) {
  std::println(outfile, "[{}: {}]{}", as_string_view(name),
               as_string_view(value),
               (flags & NGHTTP3_NV_FLAG_NEVER_INDEX) ? "(sensitive)" : "");
}
} // namespace

namespace {
void print_header(const nghttp3_rcbuf *name, const nghttp3_rcbuf *value,
                  uint8_t flags) {
  auto namebuf = nghttp3_rcbuf_get_buf(name);
  auto valuebuf = nghttp3_rcbuf_get_buf(value);
  print_header({namebuf.base, namebuf.len}, {valuebuf.base, valuebuf.len},
               flags);
}
} // namespace

namespace {
void print_header(const nghttp3_nv &nv) {
  print_header({nv.name, nv.namelen}, {nv.value, nv.valuelen}, nv.flags);
}
} // namespace

void print_http_header(int64_t stream_id, const nghttp3_rcbuf *name,
                       const nghttp3_rcbuf *value, uint8_t flags) {
  std::print(outfile, "http: stream {:#x} ", stream_id);
  print_header(name, value, flags);
}

void print_http_end_headers(int64_t stream_id) {
  std::println(outfile, "http: stream {:#x} headers ended", stream_id);
}

void print_http_data(int64_t stream_id, std::span<const uint8_t> data) {
  std::println(outfile, "http: stream {:#x} body {} bytes", stream_id,
               data.size());
  util::hexdump(outfile, data);
}

void print_http_begin_trailers(int64_t stream_id) {
  std::println(outfile, "http: stream {:#x} trailers started", stream_id);
}

void print_http_end_trailers(int64_t stream_id) {
  std::println(outfile, "http: stream {:#x} trailers ended", stream_id);
}

void print_http_request_headers(int64_t stream_id, const nghttp3_nv *nva,
                                size_t nvlen) {
  std::println(outfile, "http: stream {:#x} submit request headers", stream_id);
  for (size_t i = 0; i < nvlen; ++i) {
    auto &nv = nva[i];
    print_header(nv);
  }
}

void print_http_response_headers(int64_t stream_id, const nghttp3_nv *nva,
                                 size_t nvlen) {
  std::println(outfile, "http: stream {:#x} submit response headers",
               stream_id);
  for (size_t i = 0; i < nvlen; ++i) {
    auto &nv = nva[i];
    print_header(nv);
  }
}

void print_http_settings(const nghttp3_proto_settings *settings) {
  std::println(outfile, R"(http: remote settings
http: SETTINGS_MAX_FIELD_SECTION_SIZE={}
http: SETTINGS_QPACK_MAX_TABLE_CAPACITY={}
http: SETTINGS_QPACK_BLOCKED_STREAMS={}
http: SETTINGS_ENABLE_CONNECT_PROTOCOL={}
http: SETTINGS_H3_DATAGRAM={})",
               settings->max_field_section_size,
               settings->qpack_max_dtable_capacity,
               settings->qpack_blocked_streams,
               settings->enable_connect_protocol, settings->h3_datagram);
}

void print_http_origin(const uint8_t *origin, size_t originlen) {
  std::println(outfile, "http: origin [{}]",
               as_string_view(std::span{origin, originlen}));
}

void print_http_end_origin() { std::println(outfile, "http: origin ended"); }

std::string_view secret_title(ngtcp2_encryption_level level) {
  switch (level) {
  case NGTCP2_ENCRYPTION_LEVEL_0RTT:
    return "early_traffic"sv;
  case NGTCP2_ENCRYPTION_LEVEL_HANDSHAKE:
    return "handshake_traffic"sv;
  case NGTCP2_ENCRYPTION_LEVEL_1RTT:
    return "application_traffic"sv;
  default:
    assert(0);
    abort();
  }
}

void print_conn_info(ngtcp2_conn *conn) {
  ngtcp2_conn_info cinfo;

  ngtcp2_conn_get_conn_info(conn, &cinfo);

  std::println(
    R"(# Connection Statistics (see ngtcp2_conn_info for details)
min_rtt={}
smoothed_rtt={}
rttvar={}
cwnd={}
ssthresh={}
pkt_sent={}
bytes_sent={}
pkt_recv={}
bytes_recv={}
pkt_lost={}
bytes_lost={}
ping_recv={}
pkt_discarded={})",
    util::format_durationf(cinfo.min_rtt),
    util::format_durationf(cinfo.smoothed_rtt),
    util::format_durationf(cinfo.rttvar), cinfo.cwnd, cinfo.ssthresh,
    cinfo.pkt_sent, cinfo.bytes_sent, cinfo.pkt_recv, cinfo.bytes_recv,
    cinfo.pkt_lost, cinfo.bytes_lost, cinfo.ping_recv, cinfo.pkt_discarded);
}

} // namespace debug

} // namespace ngtcp2
