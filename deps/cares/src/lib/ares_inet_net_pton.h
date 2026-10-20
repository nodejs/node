/* MIT License
 *
 * Copyright (c) 2005 Daniel Stenberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef HEADER_CARES_INET_NET_PTON_H
#define HEADER_CARES_INET_NET_PTON_H

/*! Parse a network (an address with an optional "/bits" prefix length) from
 *  its text form.  This is not an address parser: for an address use
 *  ares_inet_pton(), which rejects a prefix length and always writes the
 *  whole address.
 *
 *  How it differs from ares_inet_pton():
 *   - A "/bits" prefix length is accepted (0-32 for IPv4, 0-128 for IPv6).
 *   - The return value is the prefix length, not 1.  Without "/bits", IPv6
 *     returns 128 and IPv4 infers it from the address class, widened to
 *     cover every octet given ("10" is 8, "130.155" is 16, "1.2.3.4" is 32,
 *     and a lone "224" is 4).  0 is a valid result ("::1/0").
 *   - What is written to dst differs by family.  On success all size bytes
 *     are zeroed first.  IPv4 then writes every octet given, including bits
 *     beyond the prefix ("1.2.3.4/8" writes 1.2.3.4).  IPv6 writes only the
 *     (bits+7)/8 bytes covering the prefix ("2001:db8::1/64" writes
 *     2001:db8::).
 *
 *  \param[in]  af    AF_INET or AF_INET6.
 *  \param[in]  src   NUL-terminated network in text form.
 *  \param[out] dst   Buffer to receive the network, in network byte order.
 *  \param[in]  size  Size of dst in bytes.
 *  \return Prefix length in bits on success.  -1 on failure, with SOCKERRNO
 *          set to ENOENT if src is not a valid network, or EMSGSIZE if it
 *          does not fit in size bytes or (IPv6) src is too long to be a
 *          network.  On Windows these are the WSA equivalents.  An
 *          unsupported af returns -1 without setting SOCKERRNO.  On failure
 *          dst may be partially written.
 */
int ares_inet_net_pton(int af, const char *src, void *dst, size_t size);

#endif /* HEADER_CARES_INET_NET_PTON_H */
