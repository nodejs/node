/*
 * ngtcp2
 *
 * Copyright (c) 2021 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions
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
#include "ngtcp2_bbr.h"

#include <assert.h>

#include "ngtcp2_log.h"
#include "ngtcp2_macro.h"
#include "ngtcp2_mem.h"
#include "ngtcp2_rcvry.h"
#include "ngtcp2_rst.h"

static const double pacing_gain_cycle[] = {1.25, 0.75, 1, 1, 1, 1, 1, 1};

#define NGTCP2_BBR_GAIN_CYCLELEN                                               \
  (sizeof(pacing_gain_cycle) / sizeof(pacing_gain_cycle[0]))

#define NGTCP2_BBR_HIGH_GAIN 2.89
#define NGTCP2_BBR_PROBE_RTT_DURATION (200 * NGTCP2_MILLISECONDS)
#define NGTCP2_RTPROP_FILTERLEN (10 * NGTCP2_SECONDS)
#define NGTCP2_BBR_BTL_BW_FILTERLEN 10

static void bbr_update_on_ack(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                              const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts);
static void bbr_update_model_and_state(ngtcp2_bbr_cc *cc,
                                       ngtcp2_conn_stat *cstat,
                                       const ngtcp2_cc_ack *ack,
                                       ngtcp2_tstamp ts);
static void bbr_update_control_parameters(ngtcp2_bbr_cc *cc,
                                          ngtcp2_conn_stat *cstat,
                                          const ngtcp2_cc_ack *ack);
static void bbr_on_transmit(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat);
static void bbr_init_round_counting(ngtcp2_bbr_cc *cc);
static void bbr_update_round(ngtcp2_bbr_cc *cc, const ngtcp2_cc_ack *ack);
static void bbr_update_btl_bw(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                              const ngtcp2_cc_ack *ack);
static void bbr_update_rtprop(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                              ngtcp2_tstamp ts);
static void bbr_init_pacing_rate(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat);
static void bbr_set_pacing_rate_with_gain(ngtcp2_bbr_cc *cc,
                                          ngtcp2_conn_stat *cstat,
                                          double pacing_gain);
static void bbr_set_pacing_rate(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat);
static void bbr_set_send_quantum(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat);
static void bbr_update_target_cwnd(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat);
static void bbr_modulate_cwnd_for_recovery(ngtcp2_bbr_cc *cc,
                                           ngtcp2_conn_stat *cstat,
                                           const ngtcp2_cc_ack *ack);
static void bbr_save_cwnd(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat);
static void bbr_restore_cwnd(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat);
static void bbr_modulate_cwnd_for_probe_rtt(ngtcp2_bbr_cc *cc,
                                            ngtcp2_conn_stat *cstat);
static void bbr_set_cwnd(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                         const ngtcp2_cc_ack *ack);
static void bbr_init(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                     ngtcp2_tstamp initial_ts);
static void bbr_enter_startup(ngtcp2_bbr_cc *cc);
static void bbr_init_full_pipe(ngtcp2_bbr_cc *cc);
static void bbr_check_full_pipe(ngtcp2_bbr_cc *cc);
static void bbr_enter_drain(ngtcp2_bbr_cc *cc);
static void bbr_check_drain(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                            ngtcp2_tstamp ts);
static void bbr_enter_probe_bw(ngtcp2_bbr_cc *cc, ngtcp2_tstamp ts);
static void bbr_check_cycle_phase(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts);
static void bbr_advance_cycle_phase(ngtcp2_bbr_cc *cc, ngtcp2_tstamp ts);
static int bbr_is_next_cycle_phase(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                   const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts);
static void bbr_handle_restart_from_idle(ngtcp2_bbr_cc *cc,
                                         ngtcp2_conn_stat *cstat);
static void bbr_check_probe_rtt(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                ngtcp2_tstamp ts);
static void bbr_enter_probe_rtt(ngtcp2_bbr_cc *cc);
static void bbr_handle_probe_rtt(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                 ngtcp2_tstamp ts);
static void bbr_exit_probe_rtt(ngtcp2_bbr_cc *cc, ngtcp2_tstamp ts);

void ngtcp2_bbr_cc_init(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                        ngtcp2_rst *rst, ngtcp2_tstamp initial_ts,
                        ngtcp2_rand rand, const ngtcp2_rand_ctx *rand_ctx,
                        ngtcp2_log *log) {
  cc->ccb.log = log;
  cc->rst = rst;
  cc->rand = rand;
  cc->rand_ctx = *rand_ctx;
  cc->initial_cwnd = cstat->cwnd;
  bbr_init(cc, cstat, initial_ts);
}

void ngtcp2_bbr_cc_free(ngtcp2_bbr_cc *cc) { (void)cc; }

int ngtcp2_cc_bbr_cc_init(ngtcp2_cc *cc, ngtcp2_log *log,
                          ngtcp2_conn_stat *cstat, ngtcp2_rst *rst,
                          ngtcp2_tstamp initial_ts, ngtcp2_rand rand,
                          const ngtcp2_rand_ctx *rand_ctx,
                          const ngtcp2_mem *mem) {
  ngtcp2_bbr_cc *bbr_cc;

  bbr_cc = ngtcp2_mem_calloc(mem, 1, sizeof(ngtcp2_bbr_cc));
  if (bbr_cc == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  ngtcp2_bbr_cc_init(bbr_cc, cstat, rst, initial_ts, rand, rand_ctx, log);

  cc->ccb = &bbr_cc->ccb;
  cc->on_pkt_acked = ngtcp2_cc_bbr_cc_on_pkt_acked;
  cc->congestion_event = ngtcp2_cc_bbr_cc_congestion_event;
  cc->on_spurious_congestion = ngtcp2_cc_bbr_cc_on_spurious_congestion;
  cc->on_persistent_congestion = ngtcp2_cc_bbr_cc_on_persistent_congestion;
  cc->on_ack_recv = ngtcp2_cc_bbr_cc_on_ack_recv;
  cc->on_pkt_sent = ngtcp2_cc_bbr_cc_on_pkt_sent;
  cc->new_rtt_sample = ngtcp2_cc_bbr_cc_new_rtt_sample;
  cc->reset = ngtcp2_cc_bbr_cc_reset;
  cc->event = ngtcp2_cc_bbr_cc_event;

  return 0;
}

void ngtcp2_cc_bbr_cc_free(ngtcp2_cc *cc, const ngtcp2_mem *mem) {
  ngtcp2_bbr_cc *bbr_cc = ngtcp2_struct_of(cc->ccb, ngtcp2_bbr_cc, ccb);

  ngtcp2_bbr_cc_free(bbr_cc);
  ngtcp2_mem_free(mem, bbr_cc);
}

void ngtcp2_cc_bbr_cc_on_pkt_acked(ngtcp2_cc *ccx, ngtcp2_conn_stat *cstat,
                                   const ngtcp2_cc_pkt *pkt, ngtcp2_tstamp ts) {
  (void)ccx;
  (void)cstat;
  (void)pkt;
  (void)ts;
}

static int in_congestion_recovery(const ngtcp2_conn_stat *cstat,
                                  ngtcp2_tstamp sent_time) {
  return cstat->congestion_recovery_start_ts != UINT64_MAX &&
         sent_time <= cstat->congestion_recovery_start_ts;
}

void ngtcp2_cc_bbr_cc_congestion_event(ngtcp2_cc *ccx, ngtcp2_conn_stat *cstat,
                                       ngtcp2_tstamp sent_ts,
                                       ngtcp2_tstamp ts) {
  ngtcp2_bbr_cc *cc = ngtcp2_struct_of(ccx->ccb, ngtcp2_bbr_cc, ccb);

  if (cc->in_loss_recovery || cc->congestion_recovery_start_ts != UINT64_MAX ||
      in_congestion_recovery(cstat, sent_ts)) {
    return;
  }

  cc->congestion_recovery_start_ts = ts;
}

void ngtcp2_cc_bbr_cc_on_spurious_congestion(ngtcp2_cc *ccx,
                                             ngtcp2_conn_stat *cstat,
                                             ngtcp2_tstamp ts) {
  ngtcp2_bbr_cc *cc = ngtcp2_struct_of(ccx->ccb, ngtcp2_bbr_cc, ccb);
  (void)ts;

  cc->congestion_recovery_start_ts = UINT64_MAX;
  cstat->congestion_recovery_start_ts = UINT64_MAX;

  if (cc->in_loss_recovery) {
    cc->in_loss_recovery = 0;
    cc->packet_conservation = 0;
    bbr_restore_cwnd(cc, cstat);
  }
}

void ngtcp2_cc_bbr_cc_on_persistent_congestion(ngtcp2_cc *ccx,
                                               ngtcp2_conn_stat *cstat,
                                               ngtcp2_tstamp ts) {
  ngtcp2_bbr_cc *cc = ngtcp2_struct_of(ccx->ccb, ngtcp2_bbr_cc, ccb);
  (void)ts;

  cstat->congestion_recovery_start_ts = UINT64_MAX;
  cc->congestion_recovery_start_ts = UINT64_MAX;
  cc->in_loss_recovery = 0;
  cc->packet_conservation = 0;

  bbr_save_cwnd(cc, cstat);
  cstat->cwnd = 2 * cstat->max_udp_payload_size;
}

void ngtcp2_cc_bbr_cc_on_ack_recv(ngtcp2_cc *ccx, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts) {
  ngtcp2_bbr_cc *bbr_cc = ngtcp2_struct_of(ccx->ccb, ngtcp2_bbr_cc, ccb);

  bbr_update_on_ack(bbr_cc, cstat, ack, ts);
}

void ngtcp2_cc_bbr_cc_on_pkt_sent(ngtcp2_cc *ccx, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_pkt *pkt) {
  ngtcp2_bbr_cc *bbr_cc = ngtcp2_struct_of(ccx->ccb, ngtcp2_bbr_cc, ccb);
  (void)pkt;

  bbr_on_transmit(bbr_cc, cstat);
}

void ngtcp2_cc_bbr_cc_new_rtt_sample(ngtcp2_cc *ccx, ngtcp2_conn_stat *cstat,
                                     ngtcp2_tstamp ts) {
  (void)ccx;
  (void)cstat;
  (void)ts;
}

void ngtcp2_cc_bbr_cc_reset(ngtcp2_cc *ccx, ngtcp2_conn_stat *cstat,
                            ngtcp2_tstamp ts) {
  ngtcp2_bbr_cc *bbr_cc = ngtcp2_struct_of(ccx->ccb, ngtcp2_bbr_cc, ccb);
  bbr_init(bbr_cc, cstat, ts);
}

void ngtcp2_cc_bbr_cc_event(ngtcp2_cc *ccx, ngtcp2_conn_stat *cstat,
                            ngtcp2_cc_event_type event, ngtcp2_tstamp ts) {
  (void)ccx;
  (void)cstat;
  (void)event;
  (void)ts;
}

static void bbr_update_on_ack(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                              const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts) {
  bbr_update_model_and_state(cc, cstat, ack, ts);
  bbr_update_control_parameters(cc, cstat, ack);
}

static void bbr_update_model_and_state(ngtcp2_bbr_cc *cc,
                                       ngtcp2_conn_stat *cstat,
                                       const ngtcp2_cc_ack *ack,
                                       ngtcp2_tstamp ts) {
  bbr_update_btl_bw(cc, cstat, ack);
  bbr_check_cycle_phase(cc, cstat, ack, ts);
  bbr_check_full_pipe(cc);
  bbr_check_drain(cc, cstat, ts);
  bbr_update_rtprop(cc, cstat, ts);
  bbr_check_probe_rtt(cc, cstat, ts);
}

static void bbr_update_control_parameters(ngtcp2_bbr_cc *cc,
                                          ngtcp2_conn_stat *cstat,
                                          const ngtcp2_cc_ack *ack) {
  bbr_set_pacing_rate(cc, cstat);
  bbr_set_send_quantum(cc, cstat);
  bbr_set_cwnd(cc, cstat, ack);
}

static void bbr_on_transmit(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat) {
  bbr_handle_restart_from_idle(cc, cstat);
}

static void bbr_init_round_counting(ngtcp2_bbr_cc *cc) {
  cc->next_round_delivered = 0;
  cc->round_start = 0;
  cc->round_count = 0;
}

static void bbr_update_round(ngtcp2_bbr_cc *cc, const ngtcp2_cc_ack *ack) {
  if (ack->pkt_delivered >= cc->next_round_delivered) {
    cc->next_round_delivered = cc->rst->delivered;
    ++cc->round_count;
    cc->round_start = 1;

    return;
  }

  cc->round_start = 0;
}

static void bbr_handle_recovery(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                const ngtcp2_cc_ack *ack) {
  if (cc->in_loss_recovery) {
    if (ack->pkt_delivered >= cc->congestion_recovery_next_round_delivered) {
      cc->packet_conservation = 0;
    }

    if (!in_congestion_recovery(cstat, ack->largest_acked_sent_ts)) {
      cc->in_loss_recovery = 0;
      cc->packet_conservation = 0;
      bbr_restore_cwnd(cc, cstat);
    }

    return;
  }

  if (cc->congestion_recovery_start_ts != UINT64_MAX) {
    cc->in_loss_recovery = 1;
    bbr_save_cwnd(cc, cstat);
    cstat->cwnd = cstat->bytes_in_flight +
                  ngtcp2_max(ack->bytes_delivered, cstat->max_udp_payload_size);

    cstat->congestion_recovery_start_ts = cc->congestion_recovery_start_ts;
    cc->congestion_recovery_start_ts = UINT64_MAX;
    cc->packet_conservation = 1;
    cc->congestion_recovery_next_round_delivered = cc->rst->delivered;
  }
}

static void bbr_update_btl_bw(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                              const ngtcp2_cc_ack *ack) {
  bbr_update_round(cc, ack);
  bbr_handle_recovery(cc, cstat, ack);

  if (cstat->delivery_rate_sec < cc->btl_bw && cc->rst->rs.is_app_limited) {
    return;
  }

  ngtcp2_window_filter_update(&cc->btl_bw_filter, cstat->delivery_rate_sec,
                              cc->round_count);

  cc->btl_bw = ngtcp2_window_filter_get_best(&cc->btl_bw_filter);
}

static void bbr_update_rtprop(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                              ngtcp2_tstamp ts) {
  cc->rtprop_expired = ts > cc->rtprop_stamp + NGTCP2_RTPROP_FILTERLEN;

  /* Need valid RTT sample */
  if (cstat->latest_rtt &&
      (cstat->latest_rtt <= cc->rt_prop || cc->rtprop_expired)) {
    cc->rt_prop = cstat->latest_rtt;
    cc->rtprop_stamp = ts;

    ngtcp2_log_info(cc->ccb.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr update RTprop=%" PRIu64, cc->rt_prop);
  }
}

static void bbr_init_pacing_rate(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat) {
  double nominal_bandwidth =
      (double)cc->initial_cwnd / (double)NGTCP2_MILLISECONDS;

  cstat->pacing_rate = cc->pacing_gain * nominal_bandwidth;
}

static void bbr_set_pacing_rate_with_gain(ngtcp2_bbr_cc *cc,
                                          ngtcp2_conn_stat *cstat,
                                          double pacing_gain) {
  double rate = pacing_gain * (double)cc->btl_bw / NGTCP2_SECONDS;

  if (cc->filled_pipe || rate > cstat->pacing_rate) {
    cstat->pacing_rate = rate;
  }
}

static void bbr_set_pacing_rate(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat) {
  bbr_set_pacing_rate_with_gain(cc, cstat, cc->pacing_gain);
}

static void bbr_set_send_quantum(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat) {
  uint64_t send_quantum;
  (void)cc;

  if (cstat->pacing_rate < 1.2 * 1024 * 1024 / 8 / NGTCP2_SECONDS) {
    cstat->send_quantum = cstat->max_udp_payload_size;
  } else if (cstat->pacing_rate < 24.0 * 1024 * 1024 / 8 / NGTCP2_SECONDS) {
    cstat->send_quantum = cstat->max_udp_payload_size * 2;
  } else {
    send_quantum =
        (uint64_t)(cstat->pacing_rate * (double)(cstat->min_rtt == UINT64_MAX
                                                     ? NGTCP2_MILLISECONDS
                                                     : cstat->min_rtt));
    cstat->send_quantum = (size_t)ngtcp2_min(send_quantum, 64 * 1024);
  }

  cstat->send_quantum =
      ngtcp2_max(cstat->send_quantum, cstat->max_udp_payload_size * 10);
}

static uint64_t bbr_inflight(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                             double gain) {
  uint64_t quanta = 3 * cstat->send_quantum;
  double estimated_bdp;

  if (cc->rt_prop == UINT64_MAX) {
    /* no valid RTT samples yet */
    return cc->initial_cwnd;
  }

  estimated_bdp = (double)cc->btl_bw * (double)cc->rt_prop / NGTCP2_SECONDS;

  return (uint64_t)(gain * estimated_bdp) + quanta;
}

static void bbr_update_target_cwnd(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat) {
  cc->target_cwnd = bbr_inflight(cc, cstat, cc->cwnd_gain);
}

static void bbr_modulate_cwnd_for_recovery(ngtcp2_bbr_cc *cc,
                                           ngtcp2_conn_stat *cstat,
                                           const ngtcp2_cc_ack *ack) {
  if (ack->bytes_lost > 0) {
    if (cstat->cwnd > ack->bytes_lost) {
      cstat->cwnd -= ack->bytes_lost;
      cstat->cwnd = ngtcp2_max(cstat->cwnd, 2 * cstat->max_udp_payload_size);
    } else {
      cstat->cwnd = cstat->max_udp_payload_size;
    }
  }

  if (cc->packet_conservation) {
    cstat->cwnd =
        ngtcp2_max(cstat->cwnd, cstat->bytes_in_flight + ack->bytes_delivered);
  }
}

static void bbr_save_cwnd(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat) {
  if (!cc->in_loss_recovery && cc->state != NGTCP2_BBR_STATE_PROBE_RTT) {
    cc->prior_cwnd = cstat->cwnd;
    return;
  }

  cc->prior_cwnd = ngtcp2_max(cc->prior_cwnd, cstat->cwnd);
}

static void bbr_restore_cwnd(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat) {
  cstat->cwnd = ngtcp2_max(cstat->cwnd, cc->prior_cwnd);
}

static uint64_t min_pipe_cwnd(size_t max_udp_payload_size) {
  return max_udp_payload_size * 4;
}

static void bbr_modulate_cwnd_for_probe_rtt(ngtcp2_bbr_cc *cc,
                                            ngtcp2_conn_stat *cstat) {
  if (cc->state == NGTCP2_BBR_STATE_PROBE_RTT) {
    cstat->cwnd =
        ngtcp2_min(cstat->cwnd, min_pipe_cwnd(cstat->max_udp_payload_size));
  }
}

static void bbr_set_cwnd(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                         const ngtcp2_cc_ack *ack) {
  bbr_update_target_cwnd(cc, cstat);
  bbr_modulate_cwnd_for_recovery(cc, cstat, ack);

  if (!cc->packet_conservation) {
    if (cc->filled_pipe) {
      cstat->cwnd =
          ngtcp2_min(cstat->cwnd + ack->bytes_delivered, cc->target_cwnd);
    } else if (cstat->cwnd < cc->target_cwnd ||
               cc->rst->delivered < cc->initial_cwnd) {
      cstat->cwnd += ack->bytes_delivered;
    }

    cstat->cwnd =
        ngtcp2_max(cstat->cwnd, min_pipe_cwnd(cstat->max_udp_payload_size));
  }

  bbr_modulate_cwnd_for_probe_rtt(cc, cstat);
}

static void bbr_init(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                     ngtcp2_tstamp initial_ts) {
  cc->pacing_gain = NGTCP2_BBR_HIGH_GAIN;
  cc->prior_cwnd = 0;
  cc->target_cwnd = 0;
  cc->btl_bw = 0;
  cc->rt_prop = UINT64_MAX;
  cc->rtprop_stamp = initial_ts;
  cc->cycle_stamp = UINT64_MAX;
  cc->probe_rtt_done_stamp = UINT64_MAX;
  cc->cycle_index = 0;
  cc->rtprop_expired = 0;
  cc->idle_restart = 0;
  cc->packet_conservation = 0;
  cc->probe_rtt_round_done = 0;

  cc->congestion_recovery_start_ts = UINT64_MAX;
  cc->congestion_recovery_next_round_delivered = 0;
  cc->in_loss_recovery = 0;

  cstat->send_quantum = cstat->max_udp_payload_size * 10;

  ngtcp2_window_filter_init(&cc->btl_bw_filter, NGTCP2_BBR_BTL_BW_FILTERLEN);

  bbr_init_round_counting(cc);
  bbr_init_full_pipe(cc);
  bbr_init_pacing_rate(cc, cstat);
  bbr_enter_startup(cc);
}

static void bbr_enter_startup(ngtcp2_bbr_cc *cc) {
  cc->state = NGTCP2_BBR_STATE_STARTUP;
  cc->pacing_gain = NGTCP2_BBR_HIGH_GAIN;
  cc->cwnd_gain = NGTCP2_BBR_HIGH_GAIN;
}

static void bbr_init_full_pipe(ngtcp2_bbr_cc *cc) {
  cc->filled_pipe = 0;
  cc->full_bw = 0;
  cc->full_bw_count = 0;
}

static void bbr_check_full_pipe(ngtcp2_bbr_cc *cc) {
  if (cc->filled_pipe || !cc->round_start || cc->rst->rs.is_app_limited) {
    /* no need to check for a full pipe now. */
    return;
  }

  /* cc->btl_bw still growing? */
  if (cc->btl_bw * 100 >= cc->full_bw * 125) {
    /* record new baseline level */
    cc->full_bw = cc->btl_bw;
    cc->full_bw_count = 0;
    return;
  }
  /* another round w/o much growth */
  ++cc->full_bw_count;
  if (cc->full_bw_count >= 3) {
    cc->filled_pipe = 1;
    ngtcp2_log_info(cc->ccb.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr filled pipe, btl_bw=%" PRIu64, cc->btl_bw);
  }
}

static void bbr_enter_drain(ngtcp2_bbr_cc *cc) {
  cc->state = NGTCP2_BBR_STATE_DRAIN;
  /* pace slowly */
  cc->pacing_gain = 1.0 / NGTCP2_BBR_HIGH_GAIN;
  /* maintain cwnd */
  cc->cwnd_gain = NGTCP2_BBR_HIGH_GAIN;
}

static void bbr_check_drain(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                            ngtcp2_tstamp ts) {
  if (cc->state == NGTCP2_BBR_STATE_STARTUP && cc->filled_pipe) {
    ngtcp2_log_info(cc->ccb.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr exit Startup and enter Drain");

    bbr_enter_drain(cc);
  }

  if (cc->state == NGTCP2_BBR_STATE_DRAIN &&
      cstat->bytes_in_flight <= bbr_inflight(cc, cstat, 1.0)) {
    ngtcp2_log_info(cc->ccb.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr exit Drain and enter ProbeBW");

    /* we estimate queue is drained */
    bbr_enter_probe_bw(cc, ts);
  }
}

static void bbr_enter_probe_bw(ngtcp2_bbr_cc *cc, ngtcp2_tstamp ts) {
  uint8_t rand;

  cc->state = NGTCP2_BBR_STATE_PROBE_BW;
  cc->pacing_gain = 1;
  cc->cwnd_gain = 2;

  assert(cc->rand);

  cc->rand(&rand, 1, &cc->rand_ctx);

  cc->cycle_index = NGTCP2_BBR_GAIN_CYCLELEN - 1 - (size_t)(rand * 7 / 256);
  bbr_advance_cycle_phase(cc, ts);
}

static void bbr_check_cycle_phase(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts) {
  if (cc->state == NGTCP2_BBR_STATE_PROBE_BW &&
      bbr_is_next_cycle_phase(cc, cstat, ack, ts)) {
    bbr_advance_cycle_phase(cc, ts);
  }
}

static void bbr_advance_cycle_phase(ngtcp2_bbr_cc *cc, ngtcp2_tstamp ts) {
  cc->cycle_stamp = ts;
  cc->cycle_index = (cc->cycle_index + 1) & (NGTCP2_BBR_GAIN_CYCLELEN - 1);
  cc->pacing_gain = pacing_gain_cycle[cc->cycle_index];
}

static int bbr_is_next_cycle_phase(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                   const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts) {
  int is_full_length = (ts - cc->cycle_stamp) > cc->rt_prop;

  if (cc->pacing_gain > 1) {
    return is_full_length && (ack->bytes_lost > 0 ||
                              ack->prior_bytes_in_flight >=
                                  bbr_inflight(cc, cstat, cc->pacing_gain));
  }

  if (cc->pacing_gain < 1) {
    return is_full_length ||
           ack->prior_bytes_in_flight <= bbr_inflight(cc, cstat, 1);
  }

  return is_full_length;
}

static void bbr_handle_restart_from_idle(ngtcp2_bbr_cc *cc,
                                         ngtcp2_conn_stat *cstat) {
  if (cstat->bytes_in_flight == 0 && cc->rst->app_limited) {
    ngtcp2_log_info(cc->ccb.log, NGTCP2_LOG_EVENT_RCV, "bbr restart from idle");

    cc->idle_restart = 1;

    if (cc->state == NGTCP2_BBR_STATE_PROBE_BW) {
      bbr_set_pacing_rate_with_gain(cc, cstat, 1);
    }
  }
}

static void bbr_check_probe_rtt(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                ngtcp2_tstamp ts) {
  if (cc->state != NGTCP2_BBR_STATE_PROBE_RTT && cc->rtprop_expired &&
      !cc->idle_restart) {
    ngtcp2_log_info(cc->ccb.log, NGTCP2_LOG_EVENT_RCV, "bbr enter ProbeRTT");

    bbr_enter_probe_rtt(cc);
    bbr_save_cwnd(cc, cstat);
    cc->probe_rtt_done_stamp = UINT64_MAX;
  }

  if (cc->state == NGTCP2_BBR_STATE_PROBE_RTT) {
    bbr_handle_probe_rtt(cc, cstat, ts);
  }

  cc->idle_restart = 0;
}

static void bbr_enter_probe_rtt(ngtcp2_bbr_cc *cc) {
  cc->state = NGTCP2_BBR_STATE_PROBE_RTT;
  cc->pacing_gain = 1;
  cc->cwnd_gain = 1;
}

static void bbr_handle_probe_rtt(ngtcp2_bbr_cc *cc, ngtcp2_conn_stat *cstat,
                                 ngtcp2_tstamp ts) {
  uint64_t app_limited = cc->rst->delivered + cstat->bytes_in_flight;

  /* Ignore low rate samples during NGTCP2_BBR_STATE_PROBE_RTT. */
  cc->rst->app_limited = app_limited ? app_limited : 1;

  if (cc->probe_rtt_done_stamp == UINT64_MAX &&
      cstat->bytes_in_flight <= min_pipe_cwnd(cstat->max_udp_payload_size)) {
    cc->probe_rtt_done_stamp = ts + NGTCP2_BBR_PROBE_RTT_DURATION;
    cc->probe_rtt_round_done = 0;
    cc->next_round_delivered = cc->rst->delivered;

    return;
  }

  if (cc->probe_rtt_done_stamp != UINT64_MAX) {
    if (cc->round_start) {
      cc->probe_rtt_round_done = 1;
    }

    if (cc->probe_rtt_round_done && ts > cc->probe_rtt_done_stamp) {
      cc->rtprop_stamp = ts;
      bbr_restore_cwnd(cc, cstat);
      bbr_exit_probe_rtt(cc, ts);
    }
  }
}

static void bbr_exit_probe_rtt(ngtcp2_bbr_cc *cc, ngtcp2_tstamp ts) {
  if (cc->filled_pipe) {
    ngtcp2_log_info(cc->ccb.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr exit ProbeRTT and enter ProbeBW");

    bbr_enter_probe_bw(cc, ts);

    return;
  }

  ngtcp2_log_info(cc->ccb.log, NGTCP2_LOG_EVENT_RCV,
                  "bbr exit ProbeRTT and enter Startup");

  bbr_enter_startup(cc);
}
