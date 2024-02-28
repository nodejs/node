/* MIT License
 *
 * Copyright (c) 2023 Brad House
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

#include "ares_setup.h"
#include "ares.h"
#include "ares_private.h"

int ares_create_query(const char *name, int dnsclass, int type,
                      unsigned short id, int rd, unsigned char **bufp,
                      int *buflenp, int max_udp_size)
{
  ares_status_t      status;
  ares_dns_record_t *dnsrec = NULL;
  size_t             len;

  if (name == NULL || bufp == NULL || buflenp == NULL) {
    status = ARES_EFORMERR;
    goto done;
  }

  *bufp    = NULL;
  *buflenp = 0;

  /* Per RFC 7686, reject queries for ".onion" domain names with NXDOMAIN. */
  if (ares__is_onion_domain(name)) {
    status = ARES_ENOTFOUND;
    goto done;
  }

  status = ares_dns_record_create(&dnsrec, id, rd ? ARES_FLAG_RD : 0,
                                  ARES_OPCODE_QUERY, ARES_RCODE_NOERROR);
  if (status != ARES_SUCCESS) {
    goto done;
  }

  status = ares_dns_record_query_add(dnsrec, name, (ares_dns_rec_type_t)type,
                                     (ares_dns_class_t)dnsclass);
  if (status != ARES_SUCCESS) {
    goto done;
  }

  /* max_udp_size > 0 indicates EDNS, so send OPT RR as an additional record */
  if (max_udp_size > 0) {
    ares_dns_rr_t *rr = NULL;

    status = ares_dns_record_rr_add(&rr, dnsrec, ARES_SECTION_ADDITIONAL, "",
                                    ARES_REC_TYPE_OPT, ARES_CLASS_IN, 0);
    if (status != ARES_SUCCESS) {
      goto done;
    }

    if (max_udp_size > 65535) {
      status = ARES_EFORMERR;
      goto done;
    }

    status = ares_dns_rr_set_u16(rr, ARES_RR_OPT_UDP_SIZE,
                                 (unsigned short)max_udp_size);
    if (status != ARES_SUCCESS) {
      goto done;
    }

    status = ares_dns_rr_set_u8(rr, ARES_RR_OPT_VERSION, 0);
    if (status != ARES_SUCCESS) {
      goto done;
    }

    status = ares_dns_rr_set_u16(rr, ARES_RR_OPT_FLAGS, 0);
    if (status != ARES_SUCCESS) {
      goto done;
    }
  }

  status = ares_dns_write(dnsrec, bufp, &len);
  if (status != ARES_SUCCESS) {
    goto done;
  }

  *buflenp = (int)len;

done:
  ares_dns_record_destroy(dnsrec);
  return (int)status;
}
