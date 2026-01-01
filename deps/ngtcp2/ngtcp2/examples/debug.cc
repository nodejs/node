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
  fprintf(outfile, "QUIC handshake has completed\n");
  return 0;
}

int handshake_confirmed(ngtcp2_conn *conn, void *user_data) {
  fprintf(outfile, "QUIC handshake has been confirmed\n");
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
  fprintf(outfile, "Ordered CRYPTO data in %s crypto level\n",
          encryption_level_str);
  util::hexdump(outfile, data.data(), data.size());
}

void print_stream_data(int64_t stream_id, std::span<const uint8_t> data) {
  fprintf(outfile, "Ordered STREAM data stream_id=0x%" PRIx64 "\n", stream_id);
  util::hexdump(outfile, data.data(), data.size());
}

void print_initial_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "initial_secret=%s\n", util::format_hex(data).c_str());
}

void print_client_in_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "client_in_secret=%s\n", util::format_hex(data).c_str());
}

void print_server_in_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "server_in_secret=%s\n", util::format_hex(data).c_str());
}

void print_handshake_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "handshake_secret=%s\n", util::format_hex(data).c_str());
}

void print_client_hs_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "client_hs_secret=%s\n", util::format_hex(data).c_str());
}

void print_server_hs_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "server_hs_secret=%s\n", util::format_hex(data).c_str());
}

void print_client_0rtt_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "client_0rtt_secret=%s\n", util::format_hex(data).c_str());
}

void print_client_1rtt_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "client_1rtt_secret=%s\n", util::format_hex(data).c_str());
}

void print_server_1rtt_secret(std::span<const uint8_t> data) {
  fprintf(outfile, "server_1rtt_secret=%s\n", util::format_hex(data).c_str());
}

void print_client_pp_key(std::span<const uint8_t> data) {
  fprintf(outfile, "+ client_pp_key=%s\n", util::format_hex(data).c_str());
}

void print_server_pp_key(std::span<const uint8_t> data) {
  fprintf(outfile, "+ server_pp_key=%s\n", util::format_hex(data).c_str());
}

void print_client_pp_iv(std::span<const uint8_t> data) {
  fprintf(outfile, "+ client_pp_iv=%s\n", util::format_hex(data).c_str());
}

void print_server_pp_iv(std::span<const uint8_t> data) {
  fprintf(outfile, "+ server_pp_iv=%s\n", util::format_hex(data).c_str());
}

void print_client_pp_hp(std::span<const uint8_t> data) {
  fprintf(outfile, "+ client_pp_hp=%s\n", util::format_hex(data).c_str());
}

void print_server_pp_hp(std::span<const uint8_t> data) {
  fprintf(outfile, "+ server_pp_hp=%s\n", util::format_hex(data).c_str());
}

void print_secrets(std::span<const uint8_t> secret,
                   std::span<const uint8_t> key, std::span<const uint8_t> iv,
                   std::span<const uint8_t> hp) {
  std::cerr << "+ secret=" << util::format_hex(secret) << "\n"
            << "+ key=" << util::format_hex(key) << "\n"
            << "+ iv=" << util::format_hex(iv) << "\n"
            << "+ hp=" << util::format_hex(hp) << std::endl;
}

void print_secrets(std::span<const uint8_t> secret,
                   std::span<const uint8_t> key, std::span<const uint8_t> iv) {
  std::cerr << "+ secret=" << util::format_hex(secret) << "\n"
            << "+ key=" << util::format_hex(key) << "\n"
            << "+ iv=" << util::format_hex(iv) << std::endl;
}

void print_hp_mask(std::span<const uint8_t> mask,
                   std::span<const uint8_t> sample) {
  fprintf(outfile, "mask=%s sample=%s\n", util::format_hex(mask).c_str(),
          util::format_hex(sample).c_str());
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

  std::cerr << "Path validation against path {local:" << local_addr
            << ", remote:" << remote_addr << "} "
            << (res == NGTCP2_PATH_VALIDATION_RESULT_SUCCESS ? "succeeded"
                                                             : "failed")
            << std::endl;
}

void print_http_begin_request_headers(int64_t stream_id) {
  fprintf(outfile, "http: stream 0x%" PRIx64 " request headers started\n",
          stream_id);
}

void print_http_begin_response_headers(int64_t stream_id) {
  fprintf(outfile, "http: stream 0x%" PRIx64 " response headers started\n",
          stream_id);
}

namespace {
void print_header(std::span<const uint8_t> name, std::span<const uint8_t> value,
                  uint8_t flags) {
  fprintf(outfile, "[%.*s: %.*s]%s\n", static_cast<int>(name.size()),
          name.data(), static_cast<int>(value.size()), value.data(),
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
  fprintf(outfile, "http: stream 0x%" PRIx64 " ", stream_id);
  print_header(name, value, flags);
}

void print_http_end_headers(int64_t stream_id) {
  fprintf(outfile, "http: stream 0x%" PRIx64 " headers ended\n", stream_id);
}

void print_http_data(int64_t stream_id, std::span<const uint8_t> data) {
  fprintf(outfile, "http: stream 0x%" PRIx64 " body %zu bytes\n", stream_id,
          data.size());
  util::hexdump(outfile, data.data(), data.size());
}

void print_http_begin_trailers(int64_t stream_id) {
  fprintf(outfile, "http: stream 0x%" PRIx64 " trailers started\n", stream_id);
}

void print_http_end_trailers(int64_t stream_id) {
  fprintf(outfile, "http: stream 0x%" PRIx64 " trailers ended\n", stream_id);
}

void print_http_request_headers(int64_t stream_id, const nghttp3_nv *nva,
                                size_t nvlen) {
  fprintf(outfile, "http: stream 0x%" PRIx64 " submit request headers\n",
          stream_id);
  for (size_t i = 0; i < nvlen; ++i) {
    auto &nv = nva[i];
    print_header(nv);
  }
}

void print_http_response_headers(int64_t stream_id, const nghttp3_nv *nva,
                                 size_t nvlen) {
  fprintf(outfile, "http: stream 0x%" PRIx64 " submit response headers\n",
          stream_id);
  for (size_t i = 0; i < nvlen; ++i) {
    auto &nv = nva[i];
    print_header(nv);
  }
}

void print_http_settings(const nghttp3_proto_settings *settings) {
  fprintf(outfile,
          "http: remote settings\n"
          "http: SETTINGS_MAX_FIELD_SECTION_SIZE=%" PRIu64 "\n"
          "http: SETTINGS_QPACK_MAX_TABLE_CAPACITY=%zu\n"
          "http: SETTINGS_QPACK_BLOCKED_STREAMS=%zu\n"
          "http: SETTINGS_ENABLE_CONNECT_PROTOCOL=%d\n"
          "http: SETTINGS_H3_DATAGRAM=%d\n",
          settings->max_field_section_size, settings->qpack_max_dtable_capacity,
          settings->qpack_blocked_streams, settings->enable_connect_protocol,
          settings->h3_datagram);
}

void print_http_origin(const uint8_t *origin, size_t originlen) {
  fprintf(outfile, "http: origin [%.*s]\n", static_cast<int>(originlen),
          origin);
}

void print_http_end_origin() { fprintf(outfile, "http: origin ended\n"); }

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

  std::cout << "# Connection Statistics (see ngtcp2_conn_info for details)\n"
               "min_rtt="
            << util::format_durationf(cinfo.min_rtt) << '\n'
            << "smoothed_rtt=" << util::format_durationf(cinfo.smoothed_rtt)
            << '\n'
            << "rttvar=" << util::format_durationf(cinfo.rttvar) << '\n'
            << "cwnd=" << cinfo.cwnd << '\n'
            << "ssthresh=" << cinfo.ssthresh << '\n'
            << "pkt_sent=" << cinfo.pkt_sent << '\n'
            << "bytes_sent=" << cinfo.bytes_sent << '\n'
            << "pkt_recv=" << cinfo.pkt_recv << '\n'
            << "bytes_recv=" << cinfo.bytes_recv << '\n'
            << "pkt_lost=" << cinfo.pkt_lost << '\n'
            << "bytes_lost=" << cinfo.bytes_lost << '\n'
            << "ping_recv=" << cinfo.ping_recv << '\n'
            << "pkt_discarded=" << cinfo.pkt_discarded << std::endl;
}

} // namespace debug

} // namespace ngtcp2
