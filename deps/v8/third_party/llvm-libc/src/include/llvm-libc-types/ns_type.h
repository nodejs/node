//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of ns_type type.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_NS_TYPE_H
#define LLVM_LIBC_TYPES_NS_TYPE_H

// DNS resource record and query types.
typedef enum __ns_type {
  ns_t_invalid = 0,
  ns_t_a = 1,
  ns_t_ns = 2,
  ns_t_md = 3,
  ns_t_mf = 4,
  ns_t_cname = 5,
  ns_t_soa = 6,
  ns_t_mb = 7,
  ns_t_mg = 8,
  ns_t_mr = 9,
  ns_t_null = 10,
  ns_t_wks = 11,
  ns_t_ptr = 12,
  ns_t_hinfo = 13,
  ns_t_minfo = 14,
  ns_t_mx = 15,
  ns_t_txt = 16,
  ns_t_rp = 17,
  ns_t_afsdb = 18,
  ns_t_x25 = 19,
  ns_t_isdn = 20,
  ns_t_rt = 21,
  ns_t_nsap = 22,
  ns_t_nsap_ptr = 23,
  ns_t_sig = 24,
  ns_t_key = 25,
  ns_t_px = 26,
  ns_t_gpos = 27,
  ns_t_aaaa = 28,
  ns_t_loc = 29,
  ns_t_nxt = 30,
  ns_t_eid = 31,
  ns_t_nimloc = 32,
  ns_t_srv = 33,
  ns_t_atma = 34,
  ns_t_naptr = 35,
  ns_t_kx = 36,
  ns_t_cert = 37,
  ns_t_a6 = 38,
  ns_t_dname = 39,
  ns_t_sink = 40,
  ns_t_opt = 41,
  ns_t_apl = 42,
  ns_t_ds = 43,
  ns_t_sshfp = 44,
  ns_t_ipseckey = 45,
  ns_t_rrsig = 46,
  ns_t_nsec = 47,
  ns_t_dnskey = 48,
  ns_t_dhcid = 49,
  ns_t_nsec3 = 50,
  ns_t_nsec3param = 51,
  ns_t_tlsa = 52,
  ns_t_smimea = 53,
  ns_t_hip = 55,
  ns_t_ninfo = 56,
  ns_t_rkey = 57,
  ns_t_talink = 58,
  ns_t_cds = 59,
  ns_t_cdnskey = 60,
  ns_t_openpgpkey = 61,
  ns_t_csync = 62,
  ns_t_spf = 99,
  ns_t_uinfo = 100,
  ns_t_uid = 101,
  ns_t_gid = 102,
  ns_t_unspec = 103,
  ns_t_nid = 104,
  ns_t_l32 = 105,
  ns_t_l64 = 106,
  ns_t_lp = 107,
  ns_t_eui48 = 108,
  ns_t_eui64 = 109,
  ns_t_tkey = 249,
  ns_t_tsig = 250,
  ns_t_ixfr = 251,
  ns_t_axfr = 252,
  ns_t_mailb = 253,
  ns_t_maila = 254,
  ns_t_any = 255,
  ns_t_uri = 256,
  ns_t_caa = 257,
  ns_t_avc = 258,
  ns_t_ta = 32768,
  ns_t_dlv = 32769,
  ns_t_max = 65536
} ns_type;

#endif // LLVM_LIBC_TYPES_NS_TYPE_H
