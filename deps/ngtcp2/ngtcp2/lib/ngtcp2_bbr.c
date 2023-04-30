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
#include <string.h>

#include "ngtcp2_log.h"
#include "ngtcp2_macro.h"
#include "ngtcp2_mem.h"
#include "ngtcp2_rcvry.h"
#include "ngtcp2_rst.h"
#include "ngtcp2_conn_stat.h"

static const double pacing_gain_cycle[] = {1.25, 0.75, 1, 1, 1, 1, 1, 1};

#define NGTCP2_BBR_GAIN_CYCLELEN ngtcp2_arraylen(pacing_gain_cycle)

#define NGTCP2_BBR_HIGH_GAIN 2.89
#define NGTCP2_BBR_PROBE_RTT_DURATION (200 * NGTCP2_MILLISECONDS)
#define NGTCP2_RTPROP_FILTERLEN (10 * NGTCP2_SECONDS)
#define NGTCP2_BBR_BTL_BW_FILTERLEN 10

static void bbr_update_on_ack(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                              const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts);
static void bbr_update_model_and_state(ngtcp2_cc_bbr *bbr,
                                       ngtcp2_conn_stat *cstat,
                                       const ngtcp2_cc_ack *ack,
                                       ngtcp2_tstamp ts);
static void bbr_update_control_parameters(ngtcp2_cc_bbr *bbr,
                                          ngtcp2_conn_stat *cstat,
                                          const ngtcp2_cc_ack *ack);
static void bbr_on_transmit(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat);
static void bbr_init_round_counting(ngtcp2_cc_bbr *bbr);
static void bbr_update_round(ngtcp2_cc_bbr *bbr, const ngtcp2_cc_ack *ack);
static void bbr_update_btl_bw(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                              const ngtcp2_cc_ack *ack);
static void bbr_update_rtprop(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                              ngtcp2_tstamp ts);
static void bbr_init_pacing_rate(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat);
static void bbr_set_pacing_rate_with_gain(ngtcp2_cc_bbr *bbr,
                                          ngtcp2_conn_stat *cstat,
                                          double pacing_gain);
static void bbr_set_pacing_rate(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat);
static void bbr_set_send_quantum(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat);
static void bbr_update_target_cwnd(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat);
static void bbr_modulate_cwnd_for_recovery(ngtcp2_cc_bbr *bbr,
                                           ngtcp2_conn_stat *cstat,
                                           const ngtcp2_cc_ack *ack);
static void bbr_save_cwnd(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat);
static void bbr_restore_cwnd(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat);
static void bbr_modulate_cwnd_for_probe_rtt(ngtcp2_cc_bbr *bbr,
                                            ngtcp2_conn_stat *cstat);
static void bbr_set_cwnd(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                         const ngtcp2_cc_ack *ack);
static void bbr_init(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                     ngtcp2_tstamp initial_ts);
static void bbr_enter_startup(ngtcp2_cc_bbr *bbr);
static void bbr_init_full_pipe(ngtcp2_cc_bbr *bbr);
static void bbr_check_full_pipe(ngtcp2_cc_bbr *bbr);
static void bbr_enter_drain(ngtcp2_cc_bbr *bbr);
static void bbr_check_drain(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                            ngtcp2_tstamp ts);
static void bbr_enter_probe_bw(ngtcp2_cc_bbr *bbr, ngtcp2_tstamp ts);
static void bbr_check_cycle_phase(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts);
static void bbr_advance_cycle_phase(ngtcp2_cc_bbr *bbr, ngtcp2_tstamp ts);
static int bbr_is_next_cycle_phase(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                   const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts);
static void bbr_handle_restart_from_idle(ngtcp2_cc_bbr *bbr,
                                         ngtcp2_conn_stat *cstat);
static void bbr_check_probe_rtt(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                ngtcp2_tstamp ts);
static void bbr_enter_probe_rtt(ngtcp2_cc_bbr *bbr);
static void bbr_handle_probe_rtt(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                 ngtcp2_tstamp ts);
static void bbr_exit_probe_rtt(ngtcp2_cc_bbr *bbr, ngtcp2_tstamp ts);

void ngtcp2_cc_bbr_init(ngtcp2_cc_bbr *bbr, ngtcp2_log *log,
                        ngtcp2_conn_stat *cstat, ngtcp2_rst *rst,
                        ngtcp2_tstamp initial_ts, ngtcp2_rand rand,
                        const ngtcp2_rand_ctx *rand_ctx) {
  memset(bbr, 0, sizeof(*bbr));

  bbr->cc.log = log;
  bbr->cc.congestion_event = ngtcp2_cc_bbr_cc_congestion_event;
  bbr->cc.on_spurious_congestion = ngtcp2_cc_bbr_cc_on_spurious_congestion;
  bbr->cc.on_persistent_congestion = ngtcp2_cc_bbr_cc_on_persistent_congestion;
  bbr->cc.on_ack_recv = ngtcp2_cc_bbr_cc_on_ack_recv;
  bbr->cc.on_pkt_sent = ngtcp2_cc_bbr_cc_on_pkt_sent;
  bbr->cc.reset = ngtcp2_cc_bbr_cc_reset;

  bbr->rst = rst;
  bbr->rand = rand;
  bbr->rand_ctx = *rand_ctx;
  bbr->initial_cwnd = cstat->cwnd;
  bbr_init(bbr, cstat, initial_ts);
}

static int in_congestion_recovery(const ngtcp2_conn_stat *cstat,
                                  ngtcp2_tstamp sent_time) {
  return cstat->congestion_recovery_start_ts != UINT64_MAX &&
         sent_time <= cstat->congestion_recovery_start_ts;
}

void ngtcp2_cc_bbr_cc_congestion_event(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                       ngtcp2_tstamp sent_ts,
                                       ngtcp2_tstamp ts) {
  ngtcp2_cc_bbr *bbr = ngtcp2_struct_of(cc, ngtcp2_cc_bbr, cc);

  if (bbr->in_loss_recovery ||
      bbr->congestion_recovery_start_ts != UINT64_MAX ||
      in_congestion_recovery(cstat, sent_ts)) {
    return;
  }

  bbr->congestion_recovery_start_ts = ts;
}

void ngtcp2_cc_bbr_cc_on_spurious_congestion(ngtcp2_cc *cc,
                                             ngtcp2_conn_stat *cstat,
                                             ngtcp2_tstamp ts) {
  ngtcp2_cc_bbr *bbr = ngtcp2_struct_of(cc, ngtcp2_cc_bbr, cc);
  (void)ts;

  bbr->congestion_recovery_start_ts = UINT64_MAX;
  cstat->congestion_recovery_start_ts = UINT64_MAX;

  if (bbr->in_loss_recovery) {
    bbr->in_loss_recovery = 0;
    bbr->packet_conservation = 0;
    bbr_restore_cwnd(bbr, cstat);
  }
}

void ngtcp2_cc_bbr_cc_on_persistent_congestion(ngtcp2_cc *cc,
                                               ngtcp2_conn_stat *cstat,
                                               ngtcp2_tstamp ts) {
  ngtcp2_cc_bbr *bbr = ngtcp2_struct_of(cc, ngtcp2_cc_bbr, cc);
  (void)ts;

  cstat->congestion_recovery_start_ts = UINT64_MAX;
  bbr->congestion_recovery_start_ts = UINT64_MAX;
  bbr->in_loss_recovery = 0;
  bbr->packet_conservation = 0;

  bbr_save_cwnd(bbr, cstat);
  cstat->cwnd = 2 * cstat->max_tx_udp_payload_size;
}

void ngtcp2_cc_bbr_cc_on_ack_recv(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts) {
  ngtcp2_cc_bbr *bbr = ngtcp2_struct_of(cc, ngtcp2_cc_bbr, cc);

  bbr_update_on_ack(bbr, cstat, ack, ts);
}

void ngtcp2_cc_bbr_cc_on_pkt_sent(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_pkt *pkt) {
  ngtcp2_cc_bbr *bbr = ngtcp2_struct_of(cc, ngtcp2_cc_bbr, cc);
  (void)pkt;

  bbr_on_transmit(bbr, cstat);
}

void ngtcp2_cc_bbr_cc_reset(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                            ngtcp2_tstamp ts) {
  ngtcp2_cc_bbr *bbr = ngtcp2_struct_of(cc, ngtcp2_cc_bbr, cc);
  bbr_init(bbr, cstat, ts);
}

static void bbr_update_on_ack(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                              const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts) {
  bbr_update_model_and_state(bbr, cstat, ack, ts);
  bbr_update_control_parameters(bbr, cstat, ack);
}

static void bbr_update_model_and_state(ngtcp2_cc_bbr *bbr,
                                       ngtcp2_conn_stat *cstat,
                                       const ngtcp2_cc_ack *ack,
                                       ngtcp2_tstamp ts) {
  bbr_update_btl_bw(bbr, cstat, ack);
  bbr_check_cycle_phase(bbr, cstat, ack, ts);
  bbr_check_full_pipe(bbr);
  bbr_check_drain(bbr, cstat, ts);
  bbr_update_rtprop(bbr, cstat, ts);
  bbr_check_probe_rtt(bbr, cstat, ts);
}

static void bbr_update_control_parameters(ngtcp2_cc_bbr *bbr,
                                          ngtcp2_conn_stat *cstat,
                                          const ngtcp2_cc_ack *ack) {
  bbr_set_pacing_rate(bbr, cstat);
  bbr_set_send_quantum(bbr, cstat);
  bbr_set_cwnd(bbr, cstat, ack);
}

static void bbr_on_transmit(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat) {
  bbr_handle_restart_from_idle(bbr, cstat);
}

static void bbr_init_round_counting(ngtcp2_cc_bbr *bbr) {
  bbr->next_round_delivered = 0;
  bbr->round_start = 0;
  bbr->round_count = 0;
}

static void bbr_update_round(ngtcp2_cc_bbr *bbr, const ngtcp2_cc_ack *ack) {
  if (ack->pkt_delivered >= bbr->next_round_delivered) {
    bbr->next_round_delivered = bbr->rst->delivered;
    ++bbr->round_count;
    bbr->round_start = 1;

    return;
  }

  bbr->round_start = 0;
}

static void bbr_handle_recovery(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                const ngtcp2_cc_ack *ack) {
  if (bbr->in_loss_recovery) {
    if (ack->pkt_delivered >= bbr->congestion_recovery_next_round_delivered) {
      bbr->packet_conservation = 0;
    }

    if (!in_congestion_recovery(cstat, ack->largest_acked_sent_ts)) {
      bbr->in_loss_recovery = 0;
      bbr->packet_conservation = 0;
      bbr_restore_cwnd(bbr, cstat);
    }

    return;
  }

  if (bbr->congestion_recovery_start_ts != UINT64_MAX) {
    bbr->in_loss_recovery = 1;
    bbr_save_cwnd(bbr, cstat);
    cstat->cwnd =
        cstat->bytes_in_flight +
        ngtcp2_max(ack->bytes_delivered, cstat->max_tx_udp_payload_size);

    cstat->congestion_recovery_start_ts = bbr->congestion_recovery_start_ts;
    bbr->congestion_recovery_start_ts = UINT64_MAX;
    bbr->packet_conservation = 1;
    bbr->congestion_recovery_next_round_delivered = bbr->rst->delivered;
  }
}

static void bbr_update_btl_bw(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                              const ngtcp2_cc_ack *ack) {
  bbr_update_round(bbr, ack);
  bbr_handle_recovery(bbr, cstat, ack);

  if (cstat->delivery_rate_sec < bbr->btl_bw && bbr->rst->rs.is_app_limited) {
    return;
  }

  ngtcp2_window_filter_update(&bbr->btl_bw_filter, cstat->delivery_rate_sec,
                              bbr->round_count);

  bbr->btl_bw = ngtcp2_window_filter_get_best(&bbr->btl_bw_filter);
}

static void bbr_update_rtprop(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                              ngtcp2_tstamp ts) {
  bbr->rtprop_expired = ts > bbr->rtprop_stamp + NGTCP2_RTPROP_FILTERLEN;

  /* Need valid RTT sample */
  if (cstat->latest_rtt &&
      (cstat->latest_rtt <= bbr->rt_prop || bbr->rtprop_expired)) {
    bbr->rt_prop = cstat->latest_rtt;
    bbr->rtprop_stamp = ts;

    ngtcp2_log_info(bbr->cc.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr update RTprop=%" PRIu64, bbr->rt_prop);
  }
}

static void bbr_init_pacing_rate(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat) {
  double nominal_bandwidth =
      (double)bbr->initial_cwnd / (double)NGTCP2_MILLISECONDS;

  cstat->pacing_rate = bbr->pacing_gain * nominal_bandwidth;
}

static void bbr_set_pacing_rate_with_gain(ngtcp2_cc_bbr *bbr,
                                          ngtcp2_conn_stat *cstat,
                                          double pacing_gain) {
  double rate = pacing_gain * (double)bbr->btl_bw / NGTCP2_SECONDS;

  if (bbr->filled_pipe || rate > cstat->pacing_rate) {
    cstat->pacing_rate = rate;
  }
}

static void bbr_set_pacing_rate(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat) {
  bbr_set_pacing_rate_with_gain(bbr, cstat, bbr->pacing_gain);
}

static void bbr_set_send_quantum(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat) {
  size_t floor, send_quantum;
  (void)bbr;

  if (cstat->pacing_rate < 1.2 * 1024 * 1024 / 8 / NGTCP2_SECONDS) {
    floor = cstat->max_tx_udp_payload_size;
  } else {
    floor = 2 * cstat->max_tx_udp_payload_size;
  }

  send_quantum = (size_t)(cstat->pacing_rate * NGTCP2_MILLISECONDS);

  send_quantum = ngtcp2_min(send_quantum, 64 * 1024);
  cstat->send_quantum = ngtcp2_max(send_quantum, floor);
}

static uint64_t bbr_inflight(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                             double gain) {
  uint64_t quanta = 3 * cstat->send_quantum;
  double estimated_bdp;

  if (bbr->rt_prop == UINT64_MAX) {
    /* no valid RTT samples yet */
    return bbr->initial_cwnd;
  }

  estimated_bdp = (double)bbr->btl_bw * (double)bbr->rt_prop / NGTCP2_SECONDS;

  return (uint64_t)(gain * estimated_bdp) + quanta;
}

static void bbr_update_target_cwnd(ngtcp2_cc_bbr *bbr,
                                   ngtcp2_conn_stat *cstat) {
  bbr->target_cwnd = bbr_inflight(bbr, cstat, bbr->cwnd_gain);
}

static void bbr_modulate_cwnd_for_recovery(ngtcp2_cc_bbr *bbr,
                                           ngtcp2_conn_stat *cstat,
                                           const ngtcp2_cc_ack *ack) {
  if (ack->bytes_lost > 0) {
    if (cstat->cwnd > ack->bytes_lost) {
      cstat->cwnd -= ack->bytes_lost;
      cstat->cwnd = ngtcp2_max(cstat->cwnd, 2 * cstat->max_tx_udp_payload_size);
    } else {
      cstat->cwnd = 2 * cstat->max_tx_udp_payload_size;
    }
  }

  if (bbr->packet_conservation) {
    cstat->cwnd =
        ngtcp2_max(cstat->cwnd, cstat->bytes_in_flight + ack->bytes_delivered);
  }
}

static void bbr_save_cwnd(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat) {
  if (!bbr->in_loss_recovery && bbr->state != NGTCP2_BBR_STATE_PROBE_RTT) {
    bbr->prior_cwnd = cstat->cwnd;
    return;
  }

  bbr->prior_cwnd = ngtcp2_max(bbr->prior_cwnd, cstat->cwnd);
}

static void bbr_restore_cwnd(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat) {
  cstat->cwnd = ngtcp2_max(cstat->cwnd, bbr->prior_cwnd);
}

static uint64_t min_pipe_cwnd(size_t max_udp_payload_size) {
  return max_udp_payload_size * 4;
}

static void bbr_modulate_cwnd_for_probe_rtt(ngtcp2_cc_bbr *bbr,
                                            ngtcp2_conn_stat *cstat) {
  if (bbr->state == NGTCP2_BBR_STATE_PROBE_RTT) {
    cstat->cwnd =
        ngtcp2_min(cstat->cwnd, min_pipe_cwnd(cstat->max_tx_udp_payload_size));
  }
}

static void bbr_set_cwnd(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                         const ngtcp2_cc_ack *ack) {
  bbr_update_target_cwnd(bbr, cstat);
  bbr_modulate_cwnd_for_recovery(bbr, cstat, ack);

  if (!bbr->packet_conservation) {
    if (bbr->filled_pipe) {
      cstat->cwnd =
          ngtcp2_min(cstat->cwnd + ack->bytes_delivered, bbr->target_cwnd);
    } else if (cstat->cwnd < bbr->target_cwnd ||
               bbr->rst->delivered < bbr->initial_cwnd) {
      cstat->cwnd += ack->bytes_delivered;
    }

    cstat->cwnd =
        ngtcp2_max(cstat->cwnd, min_pipe_cwnd(cstat->max_tx_udp_payload_size));
  }

  bbr_modulate_cwnd_for_probe_rtt(bbr, cstat);
}

static void bbr_init(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                     ngtcp2_tstamp initial_ts) {
  bbr->pacing_gain = NGTCP2_BBR_HIGH_GAIN;
  bbr->prior_cwnd = 0;
  bbr->target_cwnd = 0;
  bbr->btl_bw = 0;
  bbr->rt_prop = UINT64_MAX;
  bbr->rtprop_stamp = initial_ts;
  bbr->cycle_stamp = UINT64_MAX;
  bbr->probe_rtt_done_stamp = UINT64_MAX;
  bbr->cycle_index = 0;
  bbr->rtprop_expired = 0;
  bbr->idle_restart = 0;
  bbr->packet_conservation = 0;
  bbr->probe_rtt_round_done = 0;

  bbr->congestion_recovery_start_ts = UINT64_MAX;
  bbr->congestion_recovery_next_round_delivered = 0;
  bbr->in_loss_recovery = 0;

  cstat->send_quantum = cstat->max_tx_udp_payload_size * 10;

  ngtcp2_window_filter_init(&bbr->btl_bw_filter, NGTCP2_BBR_BTL_BW_FILTERLEN);

  bbr_init_round_counting(bbr);
  bbr_init_full_pipe(bbr);
  bbr_init_pacing_rate(bbr, cstat);
  bbr_enter_startup(bbr);
}

static void bbr_enter_startup(ngtcp2_cc_bbr *bbr) {
  bbr->state = NGTCP2_BBR_STATE_STARTUP;
  bbr->pacing_gain = NGTCP2_BBR_HIGH_GAIN;
  bbr->cwnd_gain = NGTCP2_BBR_HIGH_GAIN;
}

static void bbr_init_full_pipe(ngtcp2_cc_bbr *bbr) {
  bbr->filled_pipe = 0;
  bbr->full_bw = 0;
  bbr->full_bw_count = 0;
}

static void bbr_check_full_pipe(ngtcp2_cc_bbr *bbr) {
  if (bbr->filled_pipe || !bbr->round_start || bbr->rst->rs.is_app_limited) {
    /* no need to check for a full pipe now. */
    return;
  }

  /* bbr->btl_bw still growing? */
  if (bbr->btl_bw * 100 >= bbr->full_bw * 125) {
    /* record new baseline level */
    bbr->full_bw = bbr->btl_bw;
    bbr->full_bw_count = 0;
    return;
  }
  /* another round w/o much growth */
  ++bbr->full_bw_count;
  if (bbr->full_bw_count >= 3) {
    bbr->filled_pipe = 1;
    ngtcp2_log_info(bbr->cc.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr filled pipe, btl_bw=%" PRIu64, bbr->btl_bw);
  }
}

static void bbr_enter_drain(ngtcp2_cc_bbr *bbr) {
  bbr->state = NGTCP2_BBR_STATE_DRAIN;
  /* pace slowly */
  bbr->pacing_gain = 1.0 / NGTCP2_BBR_HIGH_GAIN;
  /* maintain cwnd */
  bbr->cwnd_gain = NGTCP2_BBR_HIGH_GAIN;
}

static void bbr_check_drain(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                            ngtcp2_tstamp ts) {
  if (bbr->state == NGTCP2_BBR_STATE_STARTUP && bbr->filled_pipe) {
    ngtcp2_log_info(bbr->cc.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr exit Startup and enter Drain");

    bbr_enter_drain(bbr);
  }

  if (bbr->state == NGTCP2_BBR_STATE_DRAIN &&
      cstat->bytes_in_flight <= bbr_inflight(bbr, cstat, 1.0)) {
    ngtcp2_log_info(bbr->cc.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr exit Drain and enter ProbeBW");

    /* we estimate queue is drained */
    bbr_enter_probe_bw(bbr, ts);
  }
}

static void bbr_enter_probe_bw(ngtcp2_cc_bbr *bbr, ngtcp2_tstamp ts) {
  uint8_t rand;

  bbr->state = NGTCP2_BBR_STATE_PROBE_BW;
  bbr->pacing_gain = 1;
  bbr->cwnd_gain = 2;

  assert(bbr->rand);

  bbr->rand(&rand, 1, &bbr->rand_ctx);

  bbr->cycle_index = NGTCP2_BBR_GAIN_CYCLELEN - 1 - (size_t)(rand * 7 / 256);
  bbr_advance_cycle_phase(bbr, ts);
}

static void bbr_check_cycle_phase(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts) {
  if (bbr->state == NGTCP2_BBR_STATE_PROBE_BW &&
      bbr_is_next_cycle_phase(bbr, cstat, ack, ts)) {
    bbr_advance_cycle_phase(bbr, ts);
  }
}

static void bbr_advance_cycle_phase(ngtcp2_cc_bbr *bbr, ngtcp2_tstamp ts) {
  bbr->cycle_stamp = ts;
  bbr->cycle_index = (bbr->cycle_index + 1) & (NGTCP2_BBR_GAIN_CYCLELEN - 1);
  bbr->pacing_gain = pacing_gain_cycle[bbr->cycle_index];
}

static int bbr_is_next_cycle_phase(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                   const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts) {
  int is_full_length = (ts - bbr->cycle_stamp) > bbr->rt_prop;

  if (bbr->pacing_gain > 1) {
    return is_full_length && (ack->bytes_lost > 0 ||
                              ack->prior_bytes_in_flight >=
                                  bbr_inflight(bbr, cstat, bbr->pacing_gain));
  }

  if (bbr->pacing_gain < 1) {
    return is_full_length ||
           ack->prior_bytes_in_flight <= bbr_inflight(bbr, cstat, 1);
  }

  return is_full_length;
}

static void bbr_handle_restart_from_idle(ngtcp2_cc_bbr *bbr,
                                         ngtcp2_conn_stat *cstat) {
  if (cstat->bytes_in_flight == 0 && bbr->rst->app_limited) {
    ngtcp2_log_info(bbr->cc.log, NGTCP2_LOG_EVENT_RCV, "bbr restart from idle");

    bbr->idle_restart = 1;

    if (bbr->state == NGTCP2_BBR_STATE_PROBE_BW) {
      bbr_set_pacing_rate_with_gain(bbr, cstat, 1);
    }
  }
}

static void bbr_check_probe_rtt(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                ngtcp2_tstamp ts) {
  if (bbr->state != NGTCP2_BBR_STATE_PROBE_RTT && bbr->rtprop_expired &&
      !bbr->idle_restart) {
    ngtcp2_log_info(bbr->cc.log, NGTCP2_LOG_EVENT_RCV, "bbr enter ProbeRTT");

    bbr_enter_probe_rtt(bbr);
    bbr_save_cwnd(bbr, cstat);
    bbr->probe_rtt_done_stamp = UINT64_MAX;
  }

  if (bbr->state == NGTCP2_BBR_STATE_PROBE_RTT) {
    bbr_handle_probe_rtt(bbr, cstat, ts);
  }

  bbr->idle_restart = 0;
}

static void bbr_enter_probe_rtt(ngtcp2_cc_bbr *bbr) {
  bbr->state = NGTCP2_BBR_STATE_PROBE_RTT;
  bbr->pacing_gain = 1;
  bbr->cwnd_gain = 1;
}

static void bbr_handle_probe_rtt(ngtcp2_cc_bbr *bbr, ngtcp2_conn_stat *cstat,
                                 ngtcp2_tstamp ts) {
  uint64_t app_limited = bbr->rst->delivered + cstat->bytes_in_flight;

  /* Ignore low rate samples during NGTCP2_BBR_STATE_PROBE_RTT. */
  bbr->rst->app_limited = app_limited ? app_limited : 1;

  if (bbr->probe_rtt_done_stamp == UINT64_MAX &&
      cstat->bytes_in_flight <= min_pipe_cwnd(cstat->max_tx_udp_payload_size)) {
    bbr->probe_rtt_done_stamp = ts + NGTCP2_BBR_PROBE_RTT_DURATION;
    bbr->probe_rtt_round_done = 0;
    bbr->next_round_delivered = bbr->rst->delivered;

    return;
  }

  if (bbr->probe_rtt_done_stamp != UINT64_MAX) {
    if (bbr->round_start) {
      bbr->probe_rtt_round_done = 1;
    }

    if (bbr->probe_rtt_round_done && ts > bbr->probe_rtt_done_stamp) {
      bbr->rtprop_stamp = ts;
      bbr_restore_cwnd(bbr, cstat);
      bbr_exit_probe_rtt(bbr, ts);
    }
  }
}

static void bbr_exit_probe_rtt(ngtcp2_cc_bbr *bbr, ngtcp2_tstamp ts) {
  if (bbr->filled_pipe) {
    ngtcp2_log_info(bbr->cc.log, NGTCP2_LOG_EVENT_RCV,
                    "bbr exit ProbeRTT and enter ProbeBW");

    bbr_enter_probe_bw(bbr, ts);

    return;
  }

  ngtcp2_log_info(bbr->cc.log, NGTCP2_LOG_EVENT_RCV,
                  "bbr exit ProbeRTT and enter Startup");

  bbr_enter_startup(bbr);
}
