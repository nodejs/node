/*
 * ngtcp2
 *
 * Copyright (c) 2023 ngtcp2 contributors
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
#include "tls_shared_picotls.h"

#include <cstdio>
#include <cstdarg>
#include <fstream>

extern std::ofstream keylog_file;

namespace {
void log_event_cb(ptls_log_event_t *self, ptls_t *ptls, const char *type,
                  const char *fmt, ...) {
  char buf[128];
  va_list ap;

  va_start(ap, fmt);
  auto len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (len < 0 || static_cast<size_t>(len) >= sizeof(buf)) {
    return;
  }

  char randhex[PTLS_HELLO_RANDOM_SIZE * 2 + 1];

  ptls_hexdump(randhex, ptls_get_client_random(ptls).base,
               PTLS_HELLO_RANDOM_SIZE);

  keylog_file << type << ' ' << randhex << ' ';
  keylog_file.write(buf, len);
  keylog_file << '\n';
  keylog_file.flush();
}
} // namespace

ptls_log_event_t log_event = {log_event_cb};
