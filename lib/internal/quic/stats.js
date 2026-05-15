'use strict';

// TODO(@jasnell) Temporarily ignoring c8 covrerage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  BigUint64Array,
  JSONStringify,
  Symbol,
  TypedArrayPrototypeSubarray,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.quic || !getOptionValue('--experimental-quic')) {
  return;
}

const {
  isArrayBuffer,
} = require('util/types');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const { inspect } = require('internal/util/inspect');
const assert = require('internal/assert');

const {
  kFinishClose,
  kInspect,
  kPrivateConstructor,
} = require('internal/quic/symbols');

// This file defines the helper objects for accessing statistics collected
// by various QUIC objects. Each of these wraps a BigUint64Array. Every
// stats object is read-only via the API, and the underlying buffer is
// only updated by the QUIC internals. When the stats object is no longer
// needed, it is closed to prevent further updates to the buffer.

const {
  // All of the IDX_STATS_* constants are the index positions of the stats
  // fields in the relevant BigUint64Array's that underlie the *Stats objects.
  // These are not exposed to end users.
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

  IDX_STATS_SESSION_CREATED_AT,
  IDX_STATS_SESSION_DESTROYED_AT,
  IDX_STATS_SESSION_CLOSING_AT,
  IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT,
  IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT,
  IDX_STATS_SESSION_BYTES_RECEIVED,
  IDX_STATS_SESSION_BIDI_IN_STREAM_COUNT,
  IDX_STATS_SESSION_BIDI_OUT_STREAM_COUNT,
  IDX_STATS_SESSION_UNI_IN_STREAM_COUNT,
  IDX_STATS_SESSION_UNI_OUT_STREAM_COUNT,
  IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT,
  IDX_STATS_SESSION_BYTES_IN_FLIGHT,
  IDX_STATS_SESSION_BLOCK_COUNT,
  IDX_STATS_SESSION_CWND,
  IDX_STATS_SESSION_LATEST_RTT,
  IDX_STATS_SESSION_MIN_RTT,
  IDX_STATS_SESSION_RTTVAR,
  IDX_STATS_SESSION_SMOOTHED_RTT,
  IDX_STATS_SESSION_SSTHRESH,
  IDX_STATS_SESSION_PKT_SENT,
  IDX_STATS_SESSION_BYTES_SENT,
  IDX_STATS_SESSION_PKT_RECV,
  IDX_STATS_SESSION_BYTES_RECV,
  IDX_STATS_SESSION_PKT_LOST,
  IDX_STATS_SESSION_BYTES_LOST,
  IDX_STATS_SESSION_PING_RECV,
  IDX_STATS_SESSION_PKT_DISCARDED,
  IDX_STATS_SESSION_DATAGRAMS_RECEIVED,
  IDX_STATS_SESSION_DATAGRAMS_SENT,
  IDX_STATS_SESSION_DATAGRAMS_ACKNOWLEDGED,
  IDX_STATS_SESSION_DATAGRAMS_LOST,
  IDX_STATS_SESSION_COUNT,

  IDX_STATS_STREAM_CREATED_AT,
  IDX_STATS_STREAM_OPENED_AT,
  IDX_STATS_STREAM_RECEIVED_AT,
  IDX_STATS_STREAM_ACKED_AT,
  IDX_STATS_STREAM_DESTROYED_AT,
  IDX_STATS_STREAM_BYTES_RECEIVED,
  IDX_STATS_STREAM_BYTES_SENT,
  IDX_STATS_STREAM_MAX_OFFSET,
  IDX_STATS_STREAM_MAX_OFFSET_ACK,
  IDX_STATS_STREAM_MAX_OFFSET_RECV,
  IDX_STATS_STREAM_FINAL_SIZE,
  IDX_STATS_STREAM_BYTES_ACCUMULATED,
  IDX_STATS_STREAM_MAX_BYTES_ACCUMULATED,
  IDX_STATS_STREAM_COUNT,
} = internalBinding('quic');

assert(IDX_STATS_ENDPOINT_CREATED_AT !== undefined);
assert(IDX_STATS_ENDPOINT_DESTROYED_AT !== undefined);
assert(IDX_STATS_ENDPOINT_BYTES_RECEIVED !== undefined);
assert(IDX_STATS_ENDPOINT_BYTES_SENT !== undefined);
assert(IDX_STATS_ENDPOINT_PACKETS_RECEIVED !== undefined);
assert(IDX_STATS_ENDPOINT_PACKETS_SENT !== undefined);
assert(IDX_STATS_ENDPOINT_SERVER_SESSIONS !== undefined);
assert(IDX_STATS_ENDPOINT_CLIENT_SESSIONS !== undefined);
assert(IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT !== undefined);
assert(IDX_STATS_ENDPOINT_RETRY_COUNT !== undefined);
assert(IDX_STATS_ENDPOINT_VERSION_NEGOTIATION_COUNT !== undefined);
assert(IDX_STATS_ENDPOINT_STATELESS_RESET_COUNT !== undefined);
assert(IDX_STATS_ENDPOINT_IMMEDIATE_CLOSE_COUNT !== undefined);
assert(IDX_STATS_SESSION_CREATED_AT !== undefined);
assert(IDX_STATS_SESSION_DESTROYED_AT !== undefined);
assert(IDX_STATS_SESSION_CLOSING_AT !== undefined);
assert(IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT !== undefined);
assert(IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT !== undefined);
assert(IDX_STATS_SESSION_BYTES_RECEIVED !== undefined);
assert(IDX_STATS_SESSION_BIDI_IN_STREAM_COUNT !== undefined);
assert(IDX_STATS_SESSION_BIDI_OUT_STREAM_COUNT !== undefined);
assert(IDX_STATS_SESSION_UNI_IN_STREAM_COUNT !== undefined);
assert(IDX_STATS_SESSION_UNI_OUT_STREAM_COUNT !== undefined);
assert(IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT !== undefined);
assert(IDX_STATS_SESSION_BYTES_IN_FLIGHT !== undefined);
assert(IDX_STATS_SESSION_BLOCK_COUNT !== undefined);
assert(IDX_STATS_SESSION_CWND !== undefined);
assert(IDX_STATS_SESSION_LATEST_RTT !== undefined);
assert(IDX_STATS_SESSION_MIN_RTT !== undefined);
assert(IDX_STATS_SESSION_RTTVAR !== undefined);
assert(IDX_STATS_SESSION_SMOOTHED_RTT !== undefined);
assert(IDX_STATS_SESSION_SSTHRESH !== undefined);
assert(IDX_STATS_SESSION_PKT_SENT !== undefined);
assert(IDX_STATS_SESSION_BYTES_SENT !== undefined);
assert(IDX_STATS_SESSION_PKT_RECV !== undefined);
assert(IDX_STATS_SESSION_BYTES_RECV !== undefined);
assert(IDX_STATS_SESSION_PKT_LOST !== undefined);
assert(IDX_STATS_SESSION_BYTES_LOST !== undefined);
assert(IDX_STATS_SESSION_PING_RECV !== undefined);
assert(IDX_STATS_SESSION_PKT_DISCARDED !== undefined);
assert(IDX_STATS_SESSION_DATAGRAMS_RECEIVED !== undefined);
assert(IDX_STATS_SESSION_DATAGRAMS_SENT !== undefined);
assert(IDX_STATS_SESSION_DATAGRAMS_ACKNOWLEDGED !== undefined);
assert(IDX_STATS_SESSION_DATAGRAMS_LOST !== undefined);
assert(IDX_STATS_STREAM_CREATED_AT !== undefined);
assert(IDX_STATS_STREAM_OPENED_AT !== undefined);
assert(IDX_STATS_STREAM_RECEIVED_AT !== undefined);
assert(IDX_STATS_STREAM_ACKED_AT !== undefined);
assert(IDX_STATS_STREAM_DESTROYED_AT !== undefined);
assert(IDX_STATS_STREAM_BYTES_RECEIVED !== undefined);
assert(IDX_STATS_STREAM_BYTES_SENT !== undefined);
assert(IDX_STATS_STREAM_MAX_OFFSET !== undefined);
assert(IDX_STATS_STREAM_MAX_OFFSET_ACK !== undefined);
assert(IDX_STATS_STREAM_MAX_OFFSET_RECV !== undefined);
assert(IDX_STATS_STREAM_FINAL_SIZE !== undefined);
assert(IDX_STATS_STREAM_BYTES_ACCUMULATED !== undefined);
assert(IDX_STATS_STREAM_MAX_BYTES_ACCUMULATED !== undefined);
assert(IDX_STATS_STREAM_COUNT !== undefined);
assert(IDX_STATS_SESSION_COUNT !== undefined);

const kCreateDisconnected = Symbol('kCreateDisconnected');

let assertIsQuicEndpointStats;
let assertIsQuicSessionStats;
let assertIsQuicStreamStats;
let isQuicEndpointStats;
let isQuicSessionStats;
let isQuicStreamStats;

function assertIsPrivateConstructor(privateSymbol) {
  if (privateSymbol !== kPrivateConstructor) {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  }
}

class QuicEndpointStats {
  /** @type {BigUint64Array} */
  #handle;
  /** @type {boolean} */
  #disconnected = false;

  static {
    isQuicEndpointStats = function(val) {
      return val != null && typeof val === 'object' && #handle in val;
    };

    assertIsQuicEndpointStats = function(val) {
      if (!isQuicEndpointStats(val)) {
        throw new ERR_INVALID_THIS('QuicEndpointStats');
      }
    };
  }

  /**
   * @param {symbol} privateSymbol
   * @param {ArrayBuffer} buffer
   */
  constructor(privateSymbol, buffer) {
    // We use the kPrivateConstructor symbol to restrict the ability to
    // create new instances of QuicEndpointStats to internal code.
    assertIsPrivateConstructor(privateSymbol);
    if (!isArrayBuffer(buffer)) {
      throw new ERR_INVALID_ARG_TYPE('buffer', ['ArrayBuffer'], buffer);
    }
    this.#handle = new BigUint64Array(buffer);
  }

  /** @type {bigint} */
  get createdAt() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_CREATED_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_DESTROYED_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bytesSent() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_BYTES_SENT];
  }

  /** @type {bigint} */
  get packetsReceived() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_PACKETS_RECEIVED];
  }

  /** @type {bigint} */
  get packetsSent() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_PACKETS_SENT];
  }

  /** @type {bigint} */
  get serverSessions() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_SERVER_SESSIONS];
  }

  /** @type {bigint} */
  get clientSessions() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_CLIENT_SESSIONS];
  }

  /** @type {bigint} */
  get serverBusyCount() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT];
  }

  /** @type {bigint} */
  get retryCount() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_RETRY_COUNT];
  }

  /** @type {bigint} */
  get versionNegotiationCount() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_VERSION_NEGOTIATION_COUNT];
  }

  /** @type {bigint} */
  get statelessResetCount() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_STATELESS_RESET_COUNT];
  }

  /** @type {bigint} */
  get immediateCloseCount() {
    assertIsQuicEndpointStats(this);
    return this.#handle[IDX_STATS_ENDPOINT_IMMEDIATE_CLOSE_COUNT];
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    assertIsQuicEndpointStats(this);
    const {
      createdAt,
      destroyedAt,
      bytesReceived,
      bytesSent,
      packetsReceived,
      packetsSent,
      serverSessions,
      clientSessions,
      serverBusyCount,
      retryCount,
      versionNegotiationCount,
      statelessResetCount,
      immediateCloseCount,
    } = this;
    return {
      __proto__: null,
      connected: this.isConnected,
      // We need to convert the values to strings because JSON does not
      // support BigInts.
      createdAt: `${createdAt}`,
      destroyedAt: `${destroyedAt}`,
      bytesReceived: `${bytesReceived}`,
      bytesSent: `${bytesSent}`,
      packetsReceived: `${packetsReceived}`,
      packetsSent: `${packetsSent}`,
      serverSessions: `${serverSessions}`,
      clientSessions: `${clientSessions}`,
      serverBusyCount: `${serverBusyCount}`,
      retryCount: `${retryCount}`,
      versionNegotiationCount: `${versionNegotiationCount}`,
      statelessResetCount: `${statelessResetCount}`,
      immediateCloseCount: `${immediateCloseCount}`,
    };
  }

  [kInspect](depth, options) {
    assertIsQuicEndpointStats(this);
    if (depth < 0) {
      return 'QuicEndpointStats { }';
    }

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    const {
      createdAt,
      destroyedAt,
      bytesReceived,
      bytesSent,
      packetsReceived,
      packetsSent,
      serverSessions,
      clientSessions,
      serverBusyCount,
      retryCount,
      versionNegotiationCount,
      statelessResetCount,
      immediateCloseCount,
    } = this;

    return `QuicEndpointStats ${inspect({
      connected: this.isConnected,
      createdAt,
      destroyedAt,
      bytesReceived,
      bytesSent,
      packetsReceived,
      packetsSent,
      serverSessions,
      clientSessions,
      serverBusyCount,
      retryCount,
      versionNegotiationCount,
      statelessResetCount,
      immediateCloseCount,
    }, opts)}`;
  }

  /**
   * True if this QuicEndpointStats object is still connected to the underlying
   * Endpoint stats source. If this returns false, then the stats object is
   * no longer being updated and should be considered stale.
   * @type {boolean}
   */
  get isConnected() {
    assertIsQuicEndpointStats(this);
    return !this.#disconnected;
  }

  [kFinishClose]() {
    this.#handle = new BigUint64Array(this.#handle);
    this.#disconnected = true;
  }
}

class QuicSessionStats {
  /** @type {BigUint64Array} */
  #handle;
  #disconnected = false;
  #offset = 0;

  static {
    isQuicSessionStats = function(val) {
      return val != null && typeof val === 'object' && #handle in val;
    };

    assertIsQuicSessionStats = function(val) {
      if (!isQuicSessionStats(val)) {
        throw new ERR_INVALID_THIS('QuicSessionStats');
      }
    };
  }


  /**
   * @param {symbol} privateSymbol
   * @param {ArrayBuffer} view
   * @param {number} [byteOffset]
   */
  constructor(privateSymbol, view, byteOffset = 0) {
    // We use the kPrivateConstructor symbol to restrict the ability to
    // create new instances of QuicSessionStats to internal code.
    assertIsPrivateConstructor(privateSymbol);
    if (isArrayBuffer(view)) {
      this.#handle = new BigUint64Array(view);
    } else {
      this.#handle = view;
    }
    this.#offset = byteOffset / 8;
  }

  /** @type {bigint} */
  get createdAt() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_CREATED_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_DESTROYED_AT];
  }

  /** @type {bigint} */
  get closingAt() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_CLOSING_AT];
  }

  /** @type {bigint} */
  get handshakeCompletedAt() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT];
  }

  /** @type {bigint} */
  get handshakeConfirmedAt() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bidiInStreamCount() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_BIDI_IN_STREAM_COUNT];
  }

  /** @type {bigint} */
  get bidiOutStreamCount() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_BIDI_OUT_STREAM_COUNT];
  }

  /** @type {bigint} */
  get uniInStreamCount() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_UNI_IN_STREAM_COUNT];
  }

  /** @type {bigint} */
  get uniOutStreamCount() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_UNI_OUT_STREAM_COUNT];
  }

  /** @type {bigint} */
  get maxBytesInFlight() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT];
  }

  /** @type {bigint} */
  get bytesInFlight() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_BYTES_IN_FLIGHT];
  }

  /** @type {bigint} */
  get blockCount() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_BLOCK_COUNT];
  }

  /** @type {bigint} */
  get cwnd() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_CWND];
  }

  /** @type {bigint} */
  get latestRtt() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_LATEST_RTT];
  }

  /** @type {bigint} */
  get minRtt() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_MIN_RTT];
  }

  /** @type {bigint} */
  get rttVar() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_RTTVAR];
  }

  /** @type {bigint} */
  get smoothedRtt() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_SMOOTHED_RTT];
  }

  /** @type {bigint} */
  get ssthresh() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_SSTHRESH];
  }

  get pktSent() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_PKT_SENT];
  }

  get bytesSent() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_BYTES_SENT];
  }

  get pktRecv() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_PKT_RECV];
  }

  get bytesRecv() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_BYTES_RECV];
  }

  get pktLost() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_PKT_LOST];
  }

  get bytesLost() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_BYTES_LOST];
  }

  get pingRecv() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_PING_RECV];
  }

  get pktDiscarded() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_PKT_DISCARDED];
  }

  /** @type {bigint} */
  get datagramsReceived() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_DATAGRAMS_RECEIVED];
  }

  /** @type {bigint} */
  get datagramsSent() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_DATAGRAMS_SENT];
  }

  /** @type {bigint} */
  get datagramsAcknowledged() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_DATAGRAMS_ACKNOWLEDGED];
  }

  /** @type {bigint} */
  get datagramsLost() {
    assertIsQuicSessionStats(this);
    return this.#handle[this.#offset + IDX_STATS_SESSION_DATAGRAMS_LOST];
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    assertIsQuicSessionStats(this);
    const {
      createdAt,
      closingAt,
      handshakeCompletedAt,
      handshakeConfirmedAt,
      bytesReceived,
      bidiInStreamCount,
      bidiOutStreamCount,
      uniInStreamCount,
      uniOutStreamCount,
      maxBytesInFlight,
      bytesInFlight,
      blockCount,
      cwnd,
      latestRtt,
      minRtt,
      rttVar,
      smoothedRtt,
      ssthresh,
      pktSent,
      bytesSent,
      pktRecv,
      bytesRecv,
      pktLost,
      bytesLost,
      pingRecv,
      pktDiscarded,
      datagramsReceived,
      datagramsSent,
      datagramsAcknowledged,
      datagramsLost,
    } = this;
    return {
      __proto__: null,
      connected: this.isConnected,
      // We need to convert the values to strings because JSON does not
      // support BigInts.
      createdAt: `${createdAt}`,
      closingAt: `${closingAt}`,
      handshakeCompletedAt: `${handshakeCompletedAt}`,
      handshakeConfirmedAt: `${handshakeConfirmedAt}`,
      bytesReceived: `${bytesReceived}`,
      bidiInStreamCount: `${bidiInStreamCount}`,
      bidiOutStreamCount: `${bidiOutStreamCount}`,
      uniInStreamCount: `${uniInStreamCount}`,
      uniOutStreamCount: `${uniOutStreamCount}`,
      maxBytesInFlight: `${maxBytesInFlight}`,
      bytesInFlight: `${bytesInFlight}`,
      blockCount: `${blockCount}`,
      cwnd: `${cwnd}`,
      latestRtt: `${latestRtt}`,
      minRtt: `${minRtt}`,
      rttVar: `${rttVar}`,
      smoothedRtt: `${smoothedRtt}`,
      ssthresh: `${ssthresh}`,
      pktSent: `${pktSent}`,
      bytesSent: `${bytesSent}`,
      pktRecv: `${pktRecv}`,
      bytesRecv: `${bytesRecv}`,
      pktLost: `${pktLost}`,
      bytesLost: `${bytesLost}`,
      pingRecv: `${pingRecv}`,
      pktDiscarded: `${pktDiscarded}`,
      datagramsReceived: `${datagramsReceived}`,
      datagramsSent: `${datagramsSent}`,
      datagramsAcknowledged: `${datagramsAcknowledged}`,
      datagramsLost: `${datagramsLost}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0) {
      return 'QuicSessionStats { }';
    }

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    const {
      createdAt,
      closingAt,
      handshakeCompletedAt,
      handshakeConfirmedAt,
      bytesReceived,
      bidiInStreamCount,
      bidiOutStreamCount,
      uniInStreamCount,
      uniOutStreamCount,
      maxBytesInFlight,
      bytesInFlight,
      blockCount,
      cwnd,
      latestRtt,
      minRtt,
      rttVar,
      smoothedRtt,
      ssthresh,
      pktSent,
      bytesSent,
      pktRecv,
      bytesRecv,
      pktLost,
      bytesLost,
      pingRecv,
      pktDiscarded,
      datagramsReceived,
      datagramsSent,
      datagramsAcknowledged,
      datagramsLost,
    } = this;

    return `QuicSessionStats ${inspect({
      connected: this.isConnected,
      createdAt,
      closingAt,
      handshakeCompletedAt,
      handshakeConfirmedAt,
      bytesReceived,
      bidiInStreamCount,
      bidiOutStreamCount,
      uniInStreamCount,
      uniOutStreamCount,
      maxBytesInFlight,
      bytesInFlight,
      blockCount,
      cwnd,
      latestRtt,
      minRtt,
      rttVar,
      smoothedRtt,
      ssthresh,
      pktSent,
      bytesSent,
      pktRecv,
      bytesRecv,
      pktLost,
      bytesLost,
      pingRecv,
      pktDiscarded,
      datagramsReceived,
      datagramsSent,
      datagramsAcknowledged,
      datagramsLost,
    }, opts)}`;
  }

  /**
   * True if this QuicSessionStats object is still connected to the underlying
   * Session stats source. If this returns false, then the stats object is
   * no longer being updated and should be considered stale.
   * @type {boolean}
   */
  get isConnected() {
    return !this.#disconnected;
  }

  [kFinishClose]() {
    const view = TypedArrayPrototypeSubarray(this.#handle,
                                             this.#offset,
                                             this.#offset + IDX_STATS_STREAM_COUNT);
    this.#handle = new BigUint64Array(view);
    this.#offset = 0;
    this.#disconnected = true;
  }
}

class QuicStreamStats {
  /** @type {BigUint64Array} */
  #handle;
  #offset = 0;
  #disconnected = false;

  static {
    isQuicStreamStats = function(val) {
      return val != null && typeof val === 'object' && #handle in val;
    };

    assertIsQuicStreamStats = function(val) {
      if (!isQuicStreamStats(val)) {
        throw new ERR_INVALID_THIS('QuicStreamStats');
      }
    };
  }

  /**
   * @param {symbol} privateSymbol
   * @param {BigUint64Array|ArrayBuffer} view
   * @param {number} [byteOffset] - byte offset into the shared page view
   */
  constructor(privateSymbol, view, byteOffset = 0) {
    // We use the kPrivateConstructor symbol to restrict the ability to
    // create new instances of QuicStreamStats to internal code.
    assertIsPrivateConstructor(privateSymbol);
    if (isArrayBuffer(view)) {
      this.#handle = new BigUint64Array(view);
    } else {
      this.#handle = view;
    }
    this.#offset = byteOffset / 8;
  }

  /** @type {bigint} */
  get createdAt() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_CREATED_AT];
  }

  /** @type {bigint} */
  get openedAt() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_OPENED_AT];
  }

  /** @type {bigint} */
  get receivedAt() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_RECEIVED_AT];
  }

  /** @type {bigint} */
  get ackedAt() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_ACKED_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_DESTROYED_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bytesSent() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_BYTES_SENT];
  }

  /** @type {bigint} */
  get maxOffset() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_MAX_OFFSET];
  }

  /** @type {bigint} */
  get maxOffsetAcknowledged() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_MAX_OFFSET_ACK];
  }

  /** @type {bigint} */
  get maxOffsetReceived() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_MAX_OFFSET_RECV];
  }

  /** @type {bigint} */
  get finalSize() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_FINAL_SIZE];
  }

  /** @type {bigint} Current bytes in the receive accumulation buffer. */
  get bytesAccumulated() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_BYTES_ACCUMULATED];
  }

  /** @type {bigint} Peak bytes accumulated over the stream's lifetime. */
  get maxBytesAccumulated() {
    assertIsQuicStreamStats(this);
    return this.#handle[this.#offset + IDX_STATS_STREAM_MAX_BYTES_ACCUMULATED];
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    assertIsQuicStreamStats(this);
    const {
      createdAt,
      openedAt,
      receivedAt,
      ackedAt,
      destroyedAt,
      bytesReceived,
      bytesSent,
      maxOffset,
      maxOffsetAcknowledged,
      maxOffsetReceived,
      finalSize,
      bytesAccumulated,
      maxBytesAccumulated,
    } = this;
    return {
      __proto__: null,
      connected: this.isConnected,
      // We need to convert the values to strings because JSON does not
      // support BigInts.
      createdAt: `${createdAt}`,
      openedAt: `${openedAt}`,
      receivedAt: `${receivedAt}`,
      ackedAt: `${ackedAt}`,
      destroyedAt: `${destroyedAt}`,
      bytesReceived: `${bytesReceived}`,
      bytesSent: `${bytesSent}`,
      maxOffset: `${maxOffset}`,
      maxOffsetAcknowledged: `${maxOffsetAcknowledged}`,
      maxOffsetReceived: `${maxOffsetReceived}`,
      finalSize: `${finalSize}`,
      bytesAccumulated: `${bytesAccumulated}`,
      maxBytesAccumulated: `${maxBytesAccumulated}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0) {
      return 'QuicStreamStats { }';
    }

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    const {
      createdAt,
      openedAt,
      receivedAt,
      ackedAt,
      destroyedAt,
      bytesReceived,
      bytesSent,
      maxOffset,
      maxOffsetAcknowledged,
      maxOffsetReceived,
      finalSize,
      bytesAccumulated,
      maxBytesAccumulated,
    } = this;

    return `QuicStreamStats ${inspect({
      connected: this.isConnected,
      createdAt,
      openedAt,
      receivedAt,
      ackedAt,
      destroyedAt,
      bytesReceived,
      bytesSent,
      maxOffset,
      maxOffsetAcknowledged,
      maxOffsetReceived,
      finalSize,
      bytesAccumulated,
      maxBytesAccumulated,
    }, opts)}`;
  }

  /**
   * True if this QuicStreamStats object is still connected to the underlying
   * Stream stats source. If this returns false, then the stats object is
   * no longer being updated and should be considered stale.
   * @type {boolean}
   */
  get isConnected() {
    return !this.#disconnected;
  }

  [kFinishClose]() {
    const view = TypedArrayPrototypeSubarray(this.#handle,
                                             this.#offset,
                                             this.#offset + IDX_STATS_STREAM_COUNT);
    this.#handle = new BigUint64Array(view);
    this.#offset = 0;
    this.#disconnected = true;
  }

  // Creates an immediately disconnected QuicStreamStats object. Used when
  // lazily creating stats for a stream that has already been destroyed.
  static [kCreateDisconnected]() {
    const count = IDX_STATS_STREAM_COUNT;
    const stats = new QuicStreamStats(kPrivateConstructor, new BigUint64Array(count), 0);
    stats.#disconnected = true;
    return stats;
  }
}

module.exports = {
  QuicEndpointStats,
  QuicSessionStats,
  QuicStreamStats,
  kCreateDisconnected,
};

/* c8 ignore stop */
