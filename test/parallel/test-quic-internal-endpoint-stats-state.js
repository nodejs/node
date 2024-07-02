// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');
const {
  strictEqual,
} = require('node:assert');

const { internalBinding } = require('internal/test/binding');
const quic = internalBinding('quic');

const {
  IDX_STATS_ENDPOINT_CREATED_AT,
  IDX_STATS_ENDPOINT_DESTROYED_AT,
  IDX_STATS_ENDPOINT_BYTES_RECEIVED,
  IDX_STATS_ENDPOINT_BYTES_SENT,
  IDX_STATS_ENDPOINT_PACKETS_RECEIVED,
  IDX_STATS_ENDPOINT_PACKETS_SENT,
  IDX_STATS_ENDPOINT_SERVER_SESSIONS,
  IDX_STATS_ENDPOINT_CLIENT_SESSIONS,
  IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT,
  IDX_STATS_ENDPOINT_RETRY_COUNT,
  IDX_STATS_ENDPOINT_VERSION_NEGOTIATION_COUNT,
  IDX_STATS_ENDPOINT_STATELESS_RESET_COUNT,
  IDX_STATS_ENDPOINT_IMMEDIATE_CLOSE_COUNT,
  IDX_STATS_ENDPOINT_COUNT,
  IDX_STATE_ENDPOINT_BOUND,
  IDX_STATE_ENDPOINT_BOUND_SIZE,
  IDX_STATE_ENDPOINT_RECEIVING,
  IDX_STATE_ENDPOINT_RECEIVING_SIZE,
  IDX_STATE_ENDPOINT_LISTENING,
  IDX_STATE_ENDPOINT_LISTENING_SIZE,
  IDX_STATE_ENDPOINT_CLOSING,
  IDX_STATE_ENDPOINT_CLOSING_SIZE,
  IDX_STATE_ENDPOINT_BUSY,
  IDX_STATE_ENDPOINT_BUSY_SIZE,
  IDX_STATE_ENDPOINT_PENDING_CALLBACKS,
  IDX_STATE_ENDPOINT_PENDING_CALLBACKS_SIZE,
} = quic;

const endpoint = new quic.Endpoint({});

const state = new DataView(endpoint.state);
strictEqual(IDX_STATE_ENDPOINT_BOUND_SIZE, 1);
strictEqual(IDX_STATE_ENDPOINT_RECEIVING_SIZE, 1);
strictEqual(IDX_STATE_ENDPOINT_LISTENING_SIZE, 1);
strictEqual(IDX_STATE_ENDPOINT_CLOSING_SIZE, 1);
strictEqual(IDX_STATE_ENDPOINT_BUSY_SIZE, 1);
strictEqual(IDX_STATE_ENDPOINT_PENDING_CALLBACKS_SIZE, 8);

strictEqual(state.getUint8(IDX_STATE_ENDPOINT_BOUND), 0);
strictEqual(state.getUint8(IDX_STATE_ENDPOINT_RECEIVING), 0);
strictEqual(state.getUint8(IDX_STATE_ENDPOINT_LISTENING), 0);
strictEqual(state.getUint8(IDX_STATE_ENDPOINT_CLOSING), 0);
strictEqual(state.getUint8(IDX_STATE_ENDPOINT_BUSY), 0);
strictEqual(state.getBigUint64(IDX_STATE_ENDPOINT_PENDING_CALLBACKS), 0n);

endpoint.markBusy(true);
strictEqual(state.getUint8(IDX_STATE_ENDPOINT_BUSY), 1);
endpoint.markBusy(false);
strictEqual(state.getUint8(IDX_STATE_ENDPOINT_BUSY), 0);

const stats = new BigUint64Array(endpoint.stats);
strictEqual(typeof stats[IDX_STATS_ENDPOINT_CREATED_AT], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_DESTROYED_AT], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_BYTES_RECEIVED], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_BYTES_SENT], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_PACKETS_RECEIVED], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_PACKETS_SENT], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_SERVER_SESSIONS], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_CLIENT_SESSIONS], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_RETRY_COUNT], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_VERSION_NEGOTIATION_COUNT], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_STATELESS_RESET_COUNT], 'bigint');
strictEqual(typeof stats[IDX_STATS_ENDPOINT_IMMEDIATE_CLOSE_COUNT], 'bigint');
strictEqual(IDX_STATS_ENDPOINT_COUNT, 13);
