//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct tcp_info for Linux.
///
/// The kernel extends this struct by appending new fields to the end. When
/// reading it with getsockopt(TCP_INFO), the returned option length indicates
/// which fields were filled in by the kernel.
///
/// This definition is based on the kernel's include/uapi/linux/tcp.h as of
/// Linux 7.3.5.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_LINUX_STRUCT_TCP_INFO_H
#define LLVM_LIBC_TYPES_LINUX_STRUCT_TCP_INFO_H

#include "../../llvm-libc-macros/stdint-macros.h"

struct tcp_info {
  uint8_t tcpi_state; // TCP_ESTABLISHED, etc.
  uint8_t tcpi_ca_state;
  uint8_t tcpi_retransmits;
  uint8_t tcpi_probes;
  uint8_t tcpi_backoff;
  uint8_t tcpi_options;
  uint8_t tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4;
  uint8_t tcpi_delivery_rate_app_limited : 1, tcpi_fastopen_client_fail : 2;

  uint32_t tcpi_rto;
  uint32_t tcpi_ato;
  uint32_t tcpi_snd_mss;
  uint32_t tcpi_rcv_mss;

  uint32_t tcpi_unacked;
  uint32_t tcpi_sacked;
  uint32_t tcpi_lost;
  uint32_t tcpi_retrans;
  uint32_t tcpi_fackets;

  // Times.
  uint32_t tcpi_last_data_sent;
  uint32_t tcpi_last_ack_sent;
  uint32_t tcpi_last_data_recv;
  uint32_t tcpi_last_ack_recv;

  // Metrics.
  uint32_t tcpi_pmtu;
  uint32_t tcpi_rcv_ssthresh;
  uint32_t tcpi_rtt;
  uint32_t tcpi_rttvar;
  uint32_t tcpi_snd_ssthresh;
  uint32_t tcpi_snd_cwnd;
  uint32_t tcpi_advmss;
  uint32_t tcpi_reordering;

  uint32_t tcpi_rcv_rtt;
  uint32_t tcpi_rcv_space;

  uint32_t tcpi_total_retrans;

  uint64_t tcpi_pacing_rate;
  uint64_t tcpi_max_pacing_rate;
  uint64_t tcpi_bytes_acked;
  uint64_t tcpi_bytes_received;
  uint32_t tcpi_segs_out;
  uint32_t tcpi_segs_in;

  uint32_t tcpi_notsent_bytes;
  uint32_t tcpi_min_rtt;
  uint32_t tcpi_data_segs_in;
  uint32_t tcpi_data_segs_out;

  uint64_t tcpi_delivery_rate;

  uint64_t tcpi_busy_time;
  uint64_t tcpi_rwnd_limited;
  uint64_t tcpi_sndbuf_limited;

  uint32_t tcpi_delivered;
  uint32_t tcpi_delivered_ce;

  uint64_t tcpi_bytes_sent;
  uint64_t tcpi_bytes_retrans;
  uint32_t tcpi_dsack_dups;
  uint32_t tcpi_reord_seen;

  uint32_t tcpi_rcv_ooopack;

  uint32_t tcpi_snd_wnd;
  uint32_t tcpi_rcv_wnd;

  uint32_t tcpi_rehash;

  uint16_t tcpi_total_rto;
  uint16_t tcpi_total_rto_recoveries;
  uint32_t tcpi_total_rto_time;

  uint32_t tcpi_received_ce;
  uint32_t tcpi_delivered_e1_bytes;
  uint32_t tcpi_delivered_e0_bytes;
  uint32_t tcpi_delivered_ce_bytes;
  uint32_t tcpi_received_e1_bytes;
  uint32_t tcpi_received_e0_bytes;
  uint32_t tcpi_received_ce_bytes;
  uint32_t tcpi_ecn_mode : 2, tcpi_accecn_opt_seen : 2,
      tcpi_accecn_fail_mode : 4, tcpi_options2 : 24;
};

#endif // LLVM_LIBC_TYPES_LINUX_STRUCT_TCP_INFO_H
