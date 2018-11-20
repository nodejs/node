/* Copyright StrongLoop, Inc. All rights reserved.
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

#ifndef S5_H_
#define S5_H_

#include <stddef.h>
#include <stdint.h>

#define S5_ERR_MAP(V)                                                         \
  V(-1, bad_version, "Bad protocol version.")                                 \
  V(-2, bad_cmd, "Bad protocol command.")                                     \
  V(-3, bad_atyp, "Bad address type.")                                        \
  V(0, ok, "No error.")                                                       \
  V(1, auth_select, "Select authentication method.")                          \
  V(2, auth_verify, "Verify authentication.")                                 \
  V(3, exec_cmd, "Execute command.")                                          \

typedef enum {
#define S5_ERR_GEN(code, name, _) s5_ ## name = code,
  S5_ERR_MAP(S5_ERR_GEN)
#undef S5_ERR_GEN
  s5_max_errors
} s5_err;

typedef enum {
  S5_AUTH_NONE = 1 << 0,
  S5_AUTH_GSSAPI = 1 << 1,
  S5_AUTH_PASSWD = 1 << 2
} s5_auth_method;

typedef enum {
  s5_auth_allow,
  s5_auth_deny
} s5_auth_result;

typedef enum {
  s5_atyp_ipv4,
  s5_atyp_ipv6,
  s5_atyp_host
} s5_atyp;

typedef enum {
  s5_cmd_tcp_connect,
  s5_cmd_tcp_bind,
  s5_cmd_udp_assoc
} s5_cmd;

typedef struct {
  uint32_t arg0;  /* Scratch space for the state machine. */
  uint32_t arg1;  /* Scratch space for the state machine. */
  uint8_t state;
  uint8_t methods;
  uint8_t cmd;
  uint8_t atyp;
  uint8_t userlen;
  uint8_t passlen;
  uint16_t dport;
  uint8_t username[257];
  uint8_t password[257];
  uint8_t daddr[257];  /* TODO(bnoordhuis) Merge with username/password. */
} s5_ctx;

void s5_init(s5_ctx *ctx);

s5_err s5_parse(s5_ctx *cx, uint8_t **data, size_t *size);

/* Only call after s5_parse() has returned s5_want_auth_method. */
unsigned int s5_auth_methods(const s5_ctx *cx);

/* Call after s5_parse() has returned s5_want_auth_method. */
int s5_select_auth(s5_ctx *cx, s5_auth_method method);

const char *s5_strerror(s5_err err);

#endif  /* S5_H_ */
