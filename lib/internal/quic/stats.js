'use strict';

const {
  Number,
  ObjectDefineProperties,
  ReflectConstruct,
  Symbol,
} = primordials;

const {
  IDX_STATS_ENDPOINT_CREATED_AT,
  IDX_STATS_ENDPOINT_DESTROYED_AT,
  IDX_STATS_ENDPOINT_BYTES_RECEIVED,
  IDX_STATS_ENDPOINT_BYTES_SENT,
  IDX_STATS_ENDPOINT_PACKETS_RECEIVED,
  IDX_STATS_ENDPOINT_PACKETS_SENT,
  IDX_STATS_ENDPOINT_SERVER_SESSIONS,
  IDX_STATS_ENDPOINT_CLIENT_SESSIONS,
  IDX_STATS_ENDPOINT_STATELESS_RESET_COUNT,
  IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT,

  IDX_STATS_SESSION_CREATED_AT,
  IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT,
  IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT,
  IDX_STATS_SESSION_SENT_AT,
  IDX_STATS_SESSION_RECEIVED_AT,
  IDX_STATS_SESSION_CLOSING_AT,
  IDX_STATS_SESSION_DESTROYED_AT,
  IDX_STATS_SESSION_BYTES_RECEIVED,
  IDX_STATS_SESSION_BYTES_SENT,
  IDX_STATS_SESSION_BIDI_STREAM_COUNT,
  IDX_STATS_SESSION_UNI_STREAM_COUNT,
  IDX_STATS_SESSION_STREAMS_IN_COUNT,
  IDX_STATS_SESSION_STREAMS_OUT_COUNT,
  IDX_STATS_SESSION_KEYUPDATE_COUNT,
  IDX_STATS_SESSION_LOSS_RETRANSMIT_COUNT,
  IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT,
  IDX_STATS_SESSION_BLOCK_COUNT,
  IDX_STATS_SESSION_BYTES_IN_FLIGHT,
  IDX_STATS_SESSION_CONGESTION_RECOVERY_START_TS,
  IDX_STATS_SESSION_CWND,
  IDX_STATS_SESSION_DELIVERY_RATE_SEC,
  IDX_STATS_SESSION_FIRST_RTT_SAMPLE_TS,
  IDX_STATS_SESSION_INITIAL_RTT,
  IDX_STATS_SESSION_LAST_TX_PKT_TS,
  IDX_STATS_SESSION_LATEST_RTT,
  IDX_STATS_SESSION_LOSS_DETECTION_TIMER,
  IDX_STATS_SESSION_LOSS_TIME,
  IDX_STATS_SESSION_MAX_UDP_PAYLOAD_SIZE,
  IDX_STATS_SESSION_MIN_RTT,
  IDX_STATS_SESSION_PTO_COUNT,
  IDX_STATS_SESSION_RTTVAR,
  IDX_STATS_SESSION_SMOOTHED_RTT,
  IDX_STATS_SESSION_SSTHRESH,
  IDX_STATS_SESSION_RECEIVE_RATE,
  IDX_STATS_SESSION_SEND_RATE,

  IDX_STATS_STREAM_CREATED_AT,
  IDX_STATS_STREAM_RECEIVED_AT,
  IDX_STATS_STREAM_ACKED_AT,
  IDX_STATS_STREAM_CLOSING_AT,
  IDX_STATS_STREAM_DESTROYED_AT,
  IDX_STATS_STREAM_BYTES_RECEIVED,
  IDX_STATS_STREAM_BYTES_SENT,
  IDX_STATS_STREAM_MAX_OFFSET,
  IDX_STATS_STREAM_MAX_OFFSET_ACK,
  IDX_STATS_STREAM_MAX_OFFSET_RECV,
  IDX_STATS_STREAM_FINAL_SIZE,
} = internalBinding('quic');

if (IDX_STATS_ENDPOINT_CREATED_AT === undefined)
  return;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  kType,
} = require('internal/quic/common');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  kEnumerableProperty,
} = require('internal/webstreams/util');

const {
  inspect,
} = require('util');

const kDetach = Symbol('kDetach');
const kDetached = Symbol('kDetached');
const kData = Symbol('kData');
const kStats = Symbol('kStats');

function isStatsBase(value, type) {
  return typeof value?.[kDetached] === 'boolean' &&
         value?.[kType] === type;
}

class StatsBase {
  [kDetached] = false;

  [kDetach]() {
    if (this[kDetached]) return;
    this[kDetached] = true;
    this[kData] = this[kData].slice();
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect(this.toJSON(), opts)}`;
  }
}

class EndpointStats extends StatsBase {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  /**
   * @returns {{
   *   createdAt: number,
   *   duration: number,
   *   bytesReceived: number,
   *   bytesSent: number,
   *   packetsReceived: number,
   *   packetsSent: number,
   *   serverSessions: number,
   *   clientSessions: number,
   *   statelessResetCount: number,
   *   serverBusyCount: number,
   * }}
   */
  toJSON() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return {
      createdAt:
        Number(this[kData][IDX_STATS_ENDPOINT_CREATED_AT]),
      duration:
        Number(calculateDuration(
          this[kData][IDX_STATS_ENDPOINT_CREATED_AT],
          this[kData][IDX_STATS_ENDPOINT_DESTROYED_AT])),
      bytesReceived:
        Number(this[kData][IDX_STATS_ENDPOINT_BYTES_RECEIVED]),
      bytesSent:
        Number(this[kData][IDX_STATS_ENDPOINT_BYTES_SENT]),
      packetsReceived:
        Number(this[kData][IDX_STATS_ENDPOINT_PACKETS_RECEIVED]),
      packetsSent:
        Number(this[kData][IDX_STATS_ENDPOINT_PACKETS_SENT]),
      serverSessions:
        Number(this[kData][IDX_STATS_ENDPOINT_SERVER_SESSIONS]),
      clientSessions:
        Number(this[kData][IDX_STATS_ENDPOINT_CLIENT_SESSIONS]),
      statelessResetCount:
        Number(this[kData][IDX_STATS_ENDPOINT_STATELESS_RESET_COUNT]),
      serverBusyCount:
        Number(this[kData][IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT]),
    };
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get createdAt() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_CREATED_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get duration() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return calculateDuration(
      this[kData][IDX_STATS_ENDPOINT_CREATED_AT],
      this[kData][IDX_STATS_ENDPOINT_DESTROYED_AT]);
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get bytesReceived() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_BYTES_RECEIVED];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get bytesSent() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_BYTES_SENT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get packetsReceived() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_PACKETS_RECEIVED];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get packetsSent() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_PACKETS_SENT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get serverSessions() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_SERVER_SESSIONS];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get clientSessions() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_CLIENT_SESSIONS];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get statelessResetCount() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_STATELESS_RESET_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get serverBusyCount() {
    if (!isStatsBase(this, 'EndpointStats'))
      throw new ERR_INVALID_THIS('EndpointStats');
    return this[kData][IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT];
  }
}

ObjectDefineProperties(EndpointStats.prototype, {
  createdAt: kEnumerableProperty,
  duration: kEnumerableProperty,
  bytesReceived: kEnumerableProperty,
  bytesSent: kEnumerableProperty,
  packetsReceived: kEnumerableProperty,
  packetsSent: kEnumerableProperty,
  serverSessions: kEnumerableProperty,
  clientSessions: kEnumerableProperty,
  statelessResetCount: kEnumerableProperty,
  serverBusyCount: kEnumerableProperty,
});

class SessionStats extends StatsBase {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  /**
   * @returns {{
   *   createdAt: number,
   *   duration: number,
   *   handshakeCompletedAt: number,
   *   handshakeConfirmedAt: number,
   *   lastSentAt: number,
   *   lastReceivedAt: number,
   *   closingAt: number,
   *   bytesReceived: number,
   *   bytesSent: number,
   *   bidiStreamCount: number,
   *   uniStreamCount: number,
   *   inboundStreamsCount: number,
   *   outboundStreamsCount: number,
   *   keyUpdateCount: number,
   *   lossRetransmitCount: number,
   *   maxBytesInFlight: number,
   *   blockCount: number,
   *   bytesInFlight: number,
   *   congestionRecoveryStartTS: number,
   *   cwnd: number,
   *   deliveryRateSec: number,
   *   firstRttSampleTS: number,
   *   initialRtt: number,
   *   lastSentPacketTS: number,
   *   latestRtt: number,
   *   lossDetectionTimer: number,
   *   lossTime: number,
   *   maxUdpPayloadSize: number,
   *   minRtt: number,
   *   ptoCount: number,
   *   rttVar: number,
   *   smoothedRtt: number,
   *   ssthresh: number,
   *   receiveRate: number,
   *   sendRate: number,
   * }}
   */
  toJSON() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return {
      createdAt:
        Number(this[kData][IDX_STATS_SESSION_CREATED_AT]),
      duration:
        Number(calculateDuration(
          this[kData][IDX_STATS_SESSION_CREATED_AT],
          this[kData][IDX_STATS_SESSION_DESTROYED_AT])),
      handshakeCompletedAt:
        Number(this[kData][IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT]),
      handshakeConfirmedAt:
        Number(this[kData][IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT]),
      lastSentAt:
        Number(this[kData][IDX_STATS_SESSION_SENT_AT]),
      lastReceivedAt:
        Number(this[kData][IDX_STATS_SESSION_RECEIVED_AT]),
      closingAt:
        Number(this[kData][IDX_STATS_SESSION_CLOSING_AT]),
      bytesReceived:
        Number(this[kData][IDX_STATS_SESSION_BYTES_RECEIVED]),
      bytesSent:
        Number(this[kData][IDX_STATS_SESSION_BYTES_SENT]),
      bidiStreamCount:
        Number(this[kData][IDX_STATS_SESSION_BIDI_STREAM_COUNT]),
      uniStreamCount:
        Number(this[kData][IDX_STATS_SESSION_UNI_STREAM_COUNT]),
      inboundStreamsCount:
        Number(this[kData][IDX_STATS_SESSION_STREAMS_IN_COUNT]),
      outboundStreamsCount:
        Number(this[kData][IDX_STATS_SESSION_STREAMS_OUT_COUNT]),
      keyUpdateCount:
        Number(this[kData][IDX_STATS_SESSION_KEYUPDATE_COUNT]),
      lossRetransmitCount:
        Number(this[kData][IDX_STATS_SESSION_LOSS_RETRANSMIT_COUNT]),
      maxBytesInFlight:
        Number(this[kData][IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT]),
      blockCount:
        Number(this[kData][IDX_STATS_SESSION_BLOCK_COUNT]),
      bytesInFlight:
        Number(this[kData][IDX_STATS_SESSION_BYTES_IN_FLIGHT]),
      congestionRecoveryStartTS:
        Number(this[kData][IDX_STATS_SESSION_CONGESTION_RECOVERY_START_TS]),
      cwnd:
        Number(this[kData][IDX_STATS_SESSION_CWND]),
      deliveryRateSec:
        Number(this[kData][IDX_STATS_SESSION_DELIVERY_RATE_SEC]),
      firstRttSampleTS:
        Number(this[kData][IDX_STATS_SESSION_FIRST_RTT_SAMPLE_TS]),
      initialRtt:
        Number(this[kData][IDX_STATS_SESSION_INITIAL_RTT]),
      lastSentPacketTS:
        Number(this[kData][IDX_STATS_SESSION_LAST_TX_PKT_TS]),
      latestRtt:
        Number(this[kData][IDX_STATS_SESSION_LATEST_RTT]),
      lossDetectionTimer:
        Number(this[kData][IDX_STATS_SESSION_LOSS_DETECTION_TIMER]),
      lossTime:
        Number(this[kData][IDX_STATS_SESSION_LOSS_TIME]),
      maxUdpPayloadSize:
        Number(this[kData][IDX_STATS_SESSION_MAX_UDP_PAYLOAD_SIZE]),
      minRtt:
        Number(this[kData][IDX_STATS_SESSION_MIN_RTT]),
      ptoCount:
        Number(this[kData][IDX_STATS_SESSION_PTO_COUNT]),
      rttVar:
        Number(this[kData][IDX_STATS_SESSION_RTTVAR]),
      smoothedRtt:
        Number(this[kData][IDX_STATS_SESSION_SMOOTHED_RTT]),
      ssthresh:
        Number(this[kData][IDX_STATS_SESSION_SSTHRESH]),
      receiveRate:
        Number(this[kData][IDX_STATS_SESSION_RECEIVE_RATE]),
      sendRate:
        Number(this[kData][IDX_STATS_SESSION_SEND_RATE]),
    };
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get createdAt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_CREATED_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get duration() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return calculateDuration(
      this[kData][IDX_STATS_SESSION_CREATED_AT],
      this[kData][IDX_STATS_SESSION_DESTROYED_AT]);
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get handshakeCompletedAt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get handshakeConfirmedAt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get lastSentAt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_SENT_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get lastReceivedAt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_RECEIVED_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get closingAt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_CLOSING_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get bytesReceived() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_BYTES_RECEIVED];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get bytesSent() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_BYTES_SENT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get bidiStreamCount() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_BIDI_STREAM_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get uniStreamCount() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_UNI_STREAM_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get inboundStreamsCount() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_STREAMS_IN_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get outboundStreamsCount() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_STREAMS_OUT_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get keyUpdateCount() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_KEYUPDATE_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get lossRetransmitCount() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_LOSS_RETRANSMIT_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get maxBytesInFlight() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get blockCount() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_BLOCK_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get bytesInFlight() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_BYTES_IN_FLIGHT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get congestionRecoveryStartTS() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_CONGESTION_RECOVERY_START_TS];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get cwnd() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_CWND];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get deliveryRateSec() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_DELIVERY_RATE_SEC];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get firstRttSampleTS() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_FIRST_RTT_SAMPLE_TS];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get initialRtt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_INITIAL_RTT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get lastSentPacketTS() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_LAST_TX_PKT_TS];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get latestRtt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_LATEST_RTT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get lossDetectionTimer() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_LOSS_DETECTION_TIMER];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get lossTime() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_LOSS_TIME];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get maxUdpPayloadSize() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_MAX_UDP_PAYLOAD_SIZE];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get minRtt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_MIN_RTT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get ptoCount() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_PTO_COUNT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get rttVar() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_RTTVAR];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get smoothedRtt() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_SMOOTHED_RTT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get ssthresh() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_SSTHRESH];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get receiveRate() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_RECEIVE_RATE];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get sendRate() {
    if (!isStatsBase(this, 'SessionStats'))
      throw new ERR_INVALID_THIS('SessionStats');
    return this[kData][IDX_STATS_SESSION_SEND_RATE];
  }
}

ObjectDefineProperties(SessionStats.prototype, {
  createdAt: kEnumerableProperty,
  duration: kEnumerableProperty,
  handshakeCompletedAt: kEnumerableProperty,
  handshakeConfirmedAt: kEnumerableProperty,
  lastSentAt: kEnumerableProperty,
  lastReceivedAt: kEnumerableProperty,
  closingAt: kEnumerableProperty,
  bytesReceived: kEnumerableProperty,
  bytesSent: kEnumerableProperty,
  bidiStreamCount: kEnumerableProperty,
  uniStreamCount: kEnumerableProperty,
  inboundStreamsCount: kEnumerableProperty,
  outboundStreamsCount: kEnumerableProperty,
  keyUpdateCount: kEnumerableProperty,
  lossRetransmitCount: kEnumerableProperty,
  maxBytesInFlight: kEnumerableProperty,
  blockCount: kEnumerableProperty,
  bytesInFlight: kEnumerableProperty,
  congestionRecoveryStartTS: kEnumerableProperty,
  cwnd: kEnumerableProperty,
  deliveryRateSec: kEnumerableProperty,
  firstRttSampleTS: kEnumerableProperty,
  initialRtt: kEnumerableProperty,
  lastSentPacketTS: kEnumerableProperty,
  latestRtt: kEnumerableProperty,
  lossDetectionTimer: kEnumerableProperty,
  lossTime: kEnumerableProperty,
  maxUdpPayloadSize: kEnumerableProperty,
  minRtt: kEnumerableProperty,
  ptoCount: kEnumerableProperty,
  rttVar: kEnumerableProperty,
  smoothedRtt: kEnumerableProperty,
  ssthresh: kEnumerableProperty,
  receiveRate: kEnumerableProperty,
  sendRate: kEnumerableProperty,
});

class StreamStats extends StatsBase {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  /**
   * @returns {{
   *   createdAt: number,
   *   duration: number,
   *   lastReceivedAt: number,
   *   lastAcknowledgeAt: number,
   *   closingAt: number,
   *   bytesReceived: number,
   *   bytesSent: number,
   *   maxOffset: number,
   *   maxOffsetAcknowledged: number,
   *   maxOffsetReceived: number,
   *   finalSize: number,
   * }}
   */
  toJSON() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return {
      createdAt:
        Number(this[kData][IDX_STATS_STREAM_CREATED_AT]),
      duration:
        Number(calculateDuration(
          this[kData][IDX_STATS_STREAM_CREATED_AT],
          this[kData][IDX_STATS_STREAM_DESTROYED_AT])),
      lastReceivedAt:
        Number(this[kData][IDX_STATS_STREAM_RECEIVED_AT]),
      lastAcknowledgeAt:
        Number(this[kData][IDX_STATS_STREAM_ACKED_AT]),
      closingAt:
        Number(this[kData][IDX_STATS_STREAM_CLOSING_AT]),
      bytesReceived:
        Number(this[kData][IDX_STATS_STREAM_BYTES_RECEIVED]),
      bytesSent:
        Number(this[kData][IDX_STATS_STREAM_BYTES_SENT]),
      maxOffset:
        Number(this[kData][IDX_STATS_STREAM_MAX_OFFSET]),
      maxOffsetAcknowledged:
        Number(this[kData][IDX_STATS_STREAM_MAX_OFFSET_ACK]),
      maxOffsetReceived:
        Number(this[kData][IDX_STATS_STREAM_MAX_OFFSET_RECV]),
      finalSize:
        Number(this[kData][IDX_STATS_STREAM_FINAL_SIZE]),
    };
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get createdAt() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_CREATED_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get duration() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return calculateDuration(
      this[kData][IDX_STATS_STREAM_CREATED_AT],
      this[kData][IDX_STATS_STREAM_DESTROYED_AT]);
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get lastReceivedAt() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_RECEIVED_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get lastAcknowledgeAt() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_ACKED_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get closingAt() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_CLOSING_AT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get bytesReceived() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_BYTES_RECEIVED];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get bytesSent() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_BYTES_SENT];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get maxOffset() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_MAX_OFFSET];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get maxOffsetAcknowledged() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_MAX_OFFSET_ACK];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get maxOffsetReceived() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_MAX_OFFSET_RECV];
  }

  /**
   * @readonly
   * @type {bigint}
   */
  get finalSize() {
    if (!isStatsBase(this, 'StreamStats'))
      throw new ERR_INVALID_THIS('StreamStats');
    return this[kData][IDX_STATS_STREAM_FINAL_SIZE];
  }
}

ObjectDefineProperties(StreamStats.prototype, {
  createdAt: kEnumerableProperty,
  duration: kEnumerableProperty,
  lastReceivedAt: kEnumerableProperty,
  lastAcknowledgeAt: kEnumerableProperty,
  closingAt: kEnumerableProperty,
  bytesReceived: kEnumerableProperty,
  bytesSent: kEnumerableProperty,
  maxOffset: kEnumerableProperty,
  maxOffsetAcknowledged: kEnumerableProperty,
  maxOffsetReceived: kEnumerableProperty,
  finalSize: kEnumerableProperty,
});

/**
 * @param {BigUint64Array} data
 */
function createEndpointStats(data) {
  return ReflectConstruct(
    class extends StatsBase {
      constructor() {
        super();
        this[kType] = 'EndpointStats';
        this[kData] = data;
      }
    }, [], EndpointStats);
}

/**
 * @param {BigUint64Array} data
 */
function createSessionStats(data) {
  return ReflectConstruct(
    class extends StatsBase {
      constructor() {
        super();
        this[kType] = 'SessionStats';
        this[kData] = data;
      }
    }, [], SessionStats);
}

/**
 * @param {BigUint64Array} data
 */
function createStreamStats(data) {
  return ReflectConstruct(
    class extends StatsBase {
      constructor() {
        super();
        this[kType] = 'StreamStats';
        this[kData] = data;
      }
    }, [], StreamStats);
}

function calculateDuration(createdAt, destroyedAt) {
  destroyedAt ||= process.hrtime.bigint();
  return destroyedAt - createdAt;
}

module.exports = {
  kDetach,
  kStats,
  EndpointStats,
  SessionStats,
  StreamStats,
  createEndpointStats,
  createSessionStats,
  createStreamStats,
};
