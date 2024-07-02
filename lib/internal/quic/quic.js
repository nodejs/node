'use strict';

const {
  ArrayIsArray,
  BigUint64Array,
  DataView,
  ObjectDefineProperties,
  ReflectConstruct,
  Symbol,
  SymbolAsyncDispose,
  SymbolDispose,
  Uint8Array,
} = primordials;

const {
  Endpoint: Endpoint_,
  setCallbacks,

  // The constants to be exposed to end users for various options.
  CC_ALGO_RENO,
  CC_ALGO_CUBIC,
  CC_ALGO_BBR,
  CC_ALGO_RENO_STR,
  CC_ALGO_CUBIC_STR,
  CC_ALGO_BBR_STR,
  PREFERRED_ADDRESS_IGNORE,
  PREFERRED_ADDRESS_USE,
  DEFAULT_PREFERRED_ADDRESS_POLICY,
  DEFAULT_CIPHERS,
  DEFAULT_GROUPS,
  STREAM_DIRECTION_BIDIRECTIONAL,
  STREAM_DIRECTION_UNIDIRECTIONAL,

  // Internal constants for use by the implementation.
  // These are not exposed to end users.
  CLOSECONTEXT_CLOSE: kCloseContextClose,
  CLOSECONTEXT_BIND_FAILURE: kCloseContextBindFailure,
  CLOSECONTEXT_LISTEN_FAILURE: kCloseContextListenFailure,
  CLOSECONTEXT_RECEIVE_FAILURE: kCloseContextReceiveFailure,
  CLOSECONTEXT_SEND_FAILURE: kCloseContextSendFailure,
  CLOSECONTEXT_START_FAILURE: kCloseContextStartFailure,

  // All of the IDX_STATS_* constants are the index positions of the stats
  // fields in the relevant BigUint64Array's that underly the *Stats objects.
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
  IDX_STATS_SESSION_CLOSING_AT,
  IDX_STATS_SESSION_DESTROYED_AT,
  IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT,
  IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT,
  IDX_STATS_SESSION_GRACEFUL_CLOSING_AT,
  IDX_STATS_SESSION_BYTES_RECEIVED,
  IDX_STATS_SESSION_BYTES_SENT,
  IDX_STATS_SESSION_BIDI_IN_STREAM_COUNT,
  IDX_STATS_SESSION_BIDI_OUT_STREAM_COUNT,
  IDX_STATS_SESSION_UNI_IN_STREAM_COUNT,
  IDX_STATS_SESSION_UNI_OUT_STREAM_COUNT,
  IDX_STATS_SESSION_LOSS_RETRANSMIT_COUNT,
  IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT,
  IDX_STATS_SESSION_BYTES_IN_FLIGHT,
  IDX_STATS_SESSION_BLOCK_COUNT,
  IDX_STATS_SESSION_CWND,
  IDX_STATS_SESSION_LATEST_RTT,
  IDX_STATS_SESSION_MIN_RTT,
  IDX_STATS_SESSION_RTTVAR,
  IDX_STATS_SESSION_SMOOTHED_RTT,
  IDX_STATS_SESSION_SSTHRESH,
  IDX_STATS_SESSION_DATAGRAMS_RECEIVED,
  IDX_STATS_SESSION_DATAGRAMS_SENT,
  IDX_STATS_SESSION_DATAGRAMS_ACKNOWLEDGED,
  IDX_STATS_SESSION_DATAGRAMS_LOST,

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

  IDX_STATE_SESSION_PATH_VALIDATION,
  IDX_STATE_SESSION_VERSION_NEGOTIATION,
  IDX_STATE_SESSION_DATAGRAM,
  IDX_STATE_SESSION_SESSION_TICKET,
  IDX_STATE_SESSION_CLOSING,
  IDX_STATE_SESSION_GRACEFUL_CLOSE,
  IDX_STATE_SESSION_SILENT_CLOSE,
  IDX_STATE_SESSION_STATELESS_RESET,
  IDX_STATE_SESSION_DESTROYED,
  IDX_STATE_SESSION_HANDSHAKE_COMPLETED,
  IDX_STATE_SESSION_HANDSHAKE_CONFIRMED,
  IDX_STATE_SESSION_STREAM_OPEN_ALLOWED,
  IDX_STATE_SESSION_PRIORITY_SUPPORTED,
  IDX_STATE_SESSION_WRAPPED,
  IDX_STATE_SESSION_LAST_DATAGRAM_ID,

  IDX_STATE_ENDPOINT_BOUND,
  IDX_STATE_ENDPOINT_RECEIVING,
  IDX_STATE_ENDPOINT_LISTENING,
  IDX_STATE_ENDPOINT_CLOSING,
  IDX_STATE_ENDPOINT_BUSY,
  IDX_STATE_ENDPOINT_PENDING_CALLBACKS,

  IDX_STATE_STREAM_ID,
  IDX_STATE_STREAM_FIN_SENT,
  IDX_STATE_STREAM_FIN_RECEIVED,
  IDX_STATE_STREAM_READ_ENDED,
  IDX_STATE_STREAM_WRITE_ENDED,
  IDX_STATE_STREAM_DESTROYED,
  IDX_STATE_STREAM_PAUSED,
  IDX_STATE_STREAM_RESET,
  IDX_STATE_STREAM_HAS_READER,
  IDX_STATE_STREAM_WANTS_BLOCK,
  IDX_STATE_STREAM_WANTS_HEADERS,
  IDX_STATE_STREAM_WANTS_RESET,
  IDX_STATE_STREAM_WANTS_TRAILERS,
} = internalBinding('quic');

const {
  isArrayBufferView,
} = require('util/types');

const {
  Buffer,
} = require('buffer');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_QUIC_CONNECTION_FAILED,
    ERR_QUIC_ENDPOINT_CLOSED,
    ERR_QUIC_OPEN_STREAM_FAILED,
  },
} = require('internal/errors');

const {
  InternalSocketAddress,
  SocketAddress,
  kHandle: kSocketAddressHandle,
} = require('internal/socketaddress');

const {
  isKeyObject,
  isCryptoKey,
} = require('internal/crypto/keys');

const {
  EventTarget,
  Event,
  kNewListener,
  kRemoveListener,
} = require('internal/event_target');

const {
  ReadableStream,
} = require('internal/webstreams/readablestream');

const {
  WritableStream,
} = require('internal/webstreams/writablestream');

const {
  customInspectSymbol: kInspect,
} = require('internal/util');
const { inspect } = require('internal/util/inspect');

const {
  kHandle: kKeyObjectHandle,
  kKeyObject: kKeyObjectInner,
} = require('internal/crypto/util');

const {
  validateObject,
} = require('internal/validators');

const kOwner = Symbol('kOwner');
const kHandle = Symbol('kHandle');
const kFinishClose = Symbol('kFinishClose');
const kStats = Symbol('kStats');
const kState = Symbol('kState');
const kReadable = Symbol('kReadable');
const kWritable = Symbol('kWritable');
const kEndpoint = Symbol('kEndpoint');
const kSession = Symbol('kSession');
const kStreams = Symbol('kStreams');
const kNewSession = Symbol('kNewSession');
const kNewStream = Symbol('kNewStream');
const kRemoteAddress = Symbol('kRemoteAddress');
const kIsPendingClose = Symbol('kIsPendingClose');
const kPendingClose = Symbol('kPendingClose');

/**
 * @typedef {import('../socketaddress.js').SocketAddress} SocketAddress
 * @typedef {import('../crypto/keys.js').KeyObject} KeyObject
 * @typedef {import('../crypto/keys.js').CryptoKey} CryptoKey
 */

/**
 * @typedef {object} EndpointOptions
 * @property {SocketAddress} [address] The local address to bind to.
 * @property {bigint|number} [retryTokenExpiration] The retry token expiration.
 * @property {bigint|number} [tokenExpiration] The token expiration.
 * @property {bigint|number} [maxConnectionsPerHost] The maximum number of connections per host
 * @property {bigint|number} [maxConnectionsTotal] The maximum number of total connections
 * @property {bigint|number} [maxStatelessResetsPerHost] The maximum number of stateless resets per host
 * @property {bigint|number} [addressLRUSize] The size of the address LRU cache
 * @property {bigint|number} [maxRetries] The maximum number of retries
 * @property {bigint|number} [maxPayloadSize] The maximum payload size
 * @property {bigint|number} [unacknowledgedPacketThreshold] The unacknowledged packet threshold
 * @property {bigint|number} [handshakeTimeout] The handshake timeout
 * @property {bigint|number} [maxStreamWindow] The maximum stream window
 * @property {bigint|number} [maxWindow] The maximum window
 * @property {number} [rxDiagnosticLoss] The receive diagnostic loss
 * @property {number} [txDiagnosticLoss] The transmit diagnostic loss
 * @property {number} [udpReceiveBufferSize] The UDP receive buffer size
 * @property {number} [udpSendBufferSize] The UDP send buffer size
 * @property {number} [udpTTL] The UDP TTL
 * @property {boolean} [noUdpPayloadSizeShaping] Disable UDP payload size shaping
 * @property {boolean} [validateAddress] Validate the address
 * @property {boolean} [disableActiveMigration] Disable active migration
 * @property {boolean} [ipv6Only] Use IPv6 only
 * @property {'reno'|'cubic'|'bbr'|number} [cc] The congestion control algorithm
 * @property {ArrayBufferView} [resetTokenSecret] The reset token secret
 * @property {ArrayBufferView} [tokenSecret] The token secret
 */

/**
 * @typedef {object} TlsOptions
 * @property {string} [sni] The server name indication
 * @property {string} [alpn] The application layer protocol negotiation
 * @property {string} [ciphers] The ciphers
 * @property {string} [groups] The groups
 * @property {boolean} [keylog] Enable key logging
 * @property {boolean} [verifyClient] Verify the client
 * @property {boolean} [tlsTrace] Enable TLS tracing
 * @property {boolean} [verifyPrivateKey] Verify the private key
 * @property {KeyObject|CryptoKey|Array<KeyObject|CryptoKey>} [keys] The keys
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [certs] The certificates
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [ca] The certificate authority
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [crl] The certificate revocation list
 */

/**
 * @typedef {object} TransportParams
 * @property {SocketAddress} [preferredAddressIpv4] The preferred IPv4 address
 * @property {SocketAddress} [preferredAddressIpv6] The preferred IPv6 address
 * @property {bigint|number} [initialMaxStreamDataBidiLocal] The initial maximum stream data bidirectional local
 * @property {bigint|number} [initialMaxStreamDataBidiRemote] The initial maximum stream data bidirectional remote
 * @property {bigint|number} [initialMaxStreamDataUni] The initial maximum stream data unidirectional
 * @property {bigint|number} [initialMaxData] The initial maximum data
 * @property {bigint|number} [initialMaxStreamsBidi] The initial maximum streams bidirectional
 * @property {bigint|number} [initialMaxStreamsUni] The initial maximum streams unidirectional
 * @property {bigint|number} [maxIdleTimeout] The maximum idle timeout
 * @property {bigint|number} [activeConnectionIDLimit] The active connection ID limit
 * @property {bigint|number} [ackDelayExponent] The acknowledgment delay exponent
 * @property {bigint|number} [maxAckDelay] The maximum acknowledgment delay
 * @property {bigint|number} [maxDatagramFrameSize] The maximum datagram frame size
 * @property {boolean} [disableActiveMigration] Disable active migration
 */

/**
 * @typedef {object} ApplicationOptions
 * @property {bigint|number} [maxHeaderPairs] The maximum header pairs
 * @property {bigint|number} [maxHeaderLength] The maximum header length
 * @property {bigint|number} [maxFieldSectionSize] The maximum field section size
 * @property {bigint|number} [qpackMaxDTableCapacity] The qpack maximum dynamic table capacity
 * @property {bigint|number} [qpackEncoderMaxDTableCapacity] The qpack encoder maximum dynamic table capacity
 * @property {bigint|number} [qpackBlockedStreams] The qpack blocked streams
 * @property {boolean} [enableConnectPrototcol] Enable the connect protocol
 * @property {boolean} [enableDatagrams] Enable datagrams
 */

/**
 * @typedef {object} SessionOptions
 * @property {number} [version] The version
 * @property {number} [minVersion] The minimum version
 * @property {'use'|'ignore'|'default'} [preferredAddressPolicy] The preferred address policy
 * @property {ApplicationOptions} [application] The application options
 * @property {TransportParams} [transportParams] The transport parameters
 * @property {TlsOptions} [tls] The TLS options
 * @property {boolean} [qlog] Enable qlog
 * @property {ArrayBufferView} [sessionTicket] The session ticket
 */

/**
 * @typedef {object} Datagrams
 * @property {ReadableStream} readable The readable stream
 * @property {WritableStream} writable The writable stream
 */

/**
 * @typedef {object} Path
 * @property {SocketAddress} local The local address
 * @property {SocketAddress} remote The remote address
 */

/**
 * Emitted when a new server-side session has been initiated.
 */
class SessionEvent extends Event {
  #session = undefined;
  #endpoint = undefined;

  /**
   * @param {Session} session
   * @param {Endpoint} endpoint
   */
  constructor(session, endpoint) {
    super('session');
    this.#session = session;
    this.#endpoint = endpoint;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {Endpoint} */
  get endpoint() { return this.#endpoint; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `SessionEvent ${inspect({
      session: this.session,
      endpoint: this.endpoint,
    }, opts)}`;
  }
}
ObjectDefineProperties(SessionEvent.prototype, {
  session: { __proto__: null, enumerable: true },
  endpoint: { __proto__: null, enumerable: true },
});

class StreamEvent extends Event {
  #stream = undefined;
  #session = undefined;

  /**
   * @param {Stream} stream
   * @param {Session} session
   */
  constructor(stream, session) {
    super('stream');
    this.#stream = stream;
    this.#session = session;
  }

  /** @type {Stream} */
  get stream() { return this.#stream; }

  /** @type {Session} */
  get session() { return this.#session; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `StreamEvent ${inspect({
      stream: this.stream,
    }, opts)}`;
  }
}
ObjectDefineProperties(StreamEvent.prototype, {
  stream: { __proto__: null, enumerable: true },
  session: { __proto__: null, enumerable: true },
});

class DatagramEvent extends Event {
  #datagram = undefined;
  #session = undefined;
  #early = false;
  constructor(datagram, session, early) {
    super('datagram');
    this.#datagram = datagram;
    this.#session = session;
    this.#early = early;
  }

  /** @type {Uint8Array} */
  get datagram() { return this.#datagram; }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {boolean} */
  get early() { return this.#early; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `DatagramEvent ${inspect({
      datagram: this.datagram,
      session: this.session,
      early: this.early,
    }, opts)}`;
  }
}
ObjectDefineProperties(DatagramEvent.prototype, {
  datagram: { __proto__: null, enumerable: true },
  session: { __proto__: null, enumerable: true },
  early: { __proto__: null, enumerable: true },
});

class DatagramStatusEvent extends Event {
  #id = undefined;
  #status = undefined;
  #session = undefined;

  constructor(id, status, session) {
    super('datagram-status');
    this.#id = id;
    this.#status = status;
    this.#session = session;
  }

  /** @type {bigint} */
  get id() { return this.#id; }

  /** @type {'lost'|'acknowledged'} */
  get status() { return this.#status; }

  /** @type {Session} */
  get session() { return this.#session; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `DatagramStatusEvent ${inspect({
      id: this.id,
      status: this.status,
      session: this.session,
    }, opts)}`;
  }
}
ObjectDefineProperties(DatagramStatusEvent.prototype, {
  id: { __proto__: null, enumerable: true },
  status: { __proto__: null, enumerable: true },
  session: { __proto__: null, enumerable: true },
});

class PathValidationEvent extends Event {
  #result = undefined;
  #newLocalAddress = undefined;
  #newRemoteAddress = undefined;
  #oldLocalAddress = undefined;
  #oldRemoteAddress = undefined;
  #preferredAddress = undefined;
  #session = undefined;

  constructor(result, newLocalAddress, newRemoteAddress,
              oldLocalAddress, oldRemoteAddress,
              preferredAddress, session) {
    super('path-validation');
    this.#result = result;
    this.#newLocalAddress = newLocalAddress;
    this.#newRemoteAddress = newRemoteAddress;
    this.#oldLocalAddress = oldLocalAddress;
    this.#oldRemoteAddress = oldRemoteAddress;
    this.#preferredAddress = preferredAddress;
    this.#session = session;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {'aborted'|'failure'|'success'} */
  get result() { return this.#result; }

  /** @type {Path} */
  get newPath() {
    return {
      local: this.#newLocalAddress,
      remote: this.#newRemoteAddress,
    };
  }

  /** @type {Path} */
  get oldPath() {
    return {
      local: this.#oldLocalAddress,
      remote: this.#oldRemoteAddress,
    };
  }

  /** @type {boolean} */
  get isPreferredAddress() { return this.#preferredAddress; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `PathValidationEvent ${inspect({
      result: this.result,
      newPath: this.newPath,
      oldPath: this.oldPath,
      isPreferredAddress: this.isPreferredAddress,
    }, opts)}`;
  }

}
ObjectDefineProperties(PathValidationEvent.prototype, {
  result: { __proto__: null, enumerable: true },
  newPath: { __proto__: null, enumerable: true },
  oldPath: { __proto__: null, enumerable: true },
  isPreferredAddress: { __proto__: null, enumerable: true },
});

class SessionTicketEvent extends Event {
  #ticket = undefined;
  #session = undefined;

  constructor(ticket, session) {
    super('session-ticket');
    this.#ticket = ticket;
    this.#session = session;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {object} */
  get ticket() { return this.#ticket; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `SessionTicketEvent ${inspect({
      ticket: this.ticket,
      session: this.session,
    }, opts)}`;
  }
}
ObjectDefineProperties(SessionTicketEvent.prototype, {
  ticket: { __proto__: null, enumerable: true },
  session: { __proto__: null, enumerable: true },
});

class VersionNegotiationEvent extends Event {
  #session = undefined;
  #version = undefined;
  #requestedVersions = undefined;
  #supportedVersions = undefined;

  constructor(version, requestedVersions, supportedVersions, session) {
    super('version-negotiation');
    this.#session = session;
    this.#version = version;
    this.#requestedVersions = requestedVersions;
    this.#supportedVersions = supportedVersions;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {number} */
  get version() { return this.#version; }

  /** @type {Array<number>} */
  get requestedVersions() { return this.#requestedVersions; }

  /** @type {Array<number>} */
  get supportedVersions() { return this.#supportedVersions; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `VersionNegotiationEvent ${inspect({
      version: this.version,
      requestedVersions: this.requestedVersions,
      supportedVersions: this.supportedVersions,
      session: this.session,
    }, opts)}`;
  }
}
ObjectDefineProperties(VersionNegotiationEvent.prototype, {
  version: { __proto__: null, enumerable: true },
  requestedVersions: { __proto__: null, enumerable: true },
  supportedVersions: { __proto__: null, enumerable: true },
  session: { __proto__: null, enumerable: true },
});

class HandshakeEvent extends Event {
  #session = undefined;
  #sni = undefined;
  #alpn = undefined;
  #cipher = undefined;
  #cipherVersion = undefined;
  #validationErrorReason = undefined;
  #validationErrorCode = undefined;
  #earlyDataAccepted = false;

  constructor(sni, alpn, cipher, cipherVersion,
              validationErrorReason, validationErrorCode,
              earlyDataAccepted, session) {
    super('handshake');
    this.#session = session;
    this.#sni = sni;
    this.#alpn = alpn;
    this.#cipher = cipher;
    this.#cipherVersion = cipherVersion;
    this.#validationErrorReason = validationErrorReason;
    this.#validationErrorCode = validationErrorCode;
    this.#earlyDataAccepted = earlyDataAccepted;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {string} */
  get sni() { return this.#sni; }

  /** @type {string} */
  get alpn() { return this.#alpn; }

  /** @type {string} */
  get cipher() { return this.#cipher; }

  /** @type {string} */
  get cipherVersion() { return this.#cipherVersion; }

  /** @type {string} */
  get validationErrorReason() { return this.#validationErrorReason; }

  /** @type {number} */
  get validationErrorCode() { return this.#validationErrorCode; }

  /** @type {boolean} */
  get earlyDataAccepted() { return this.#earlyDataAccepted; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `HandshakeEvent ${inspect({
      sni: this.sni,
      alpn: this.alpn,
      cipher: this.cipher,
      cipherVersion: this.cipherVersion,
      validationErrorReason: this.validationErrorReason,
      validationErrorCode: this.validationErrorCode,
      earlyDataAccepted: this.earlyDataAccepted,
      session: this.session,
    }, opts)}`;
  }
}
ObjectDefineProperties(HandshakeEvent.prototype, {
  sni: { __proto__: null, enumerable: true },
  alpn: { __proto__: null, enumerable: true },
  cipher: { __proto__: null, enumerable: true },
  cipherVersion: { __proto__: null, enumerable: true },
  validationErrorReason: { __proto__: null, enumerable: true },
  validationErrorCode: { __proto__: null, enumerable: true },
  earlyDataAccepted: { __proto__: null, enumerable: true },
  session: { __proto__: null, enumerable: true },
});

setCallbacks({
  onEndpointClose(context, status) {
    this[kOwner][kFinishClose](context, status);
  },
  onSessionNew(session) {
    this[kOwner][kNewSession](session);
  },
  onSessionClose(errorType, code, reason) {
    this[kOwner][kFinishClose](errorType, code, reason);
  },
  onSessionDatagram(uint8Array, early) {
    const session = this[kOwner];
    // The event is ignored if the session has been destroyed.
    if (session.destroyed) return;
    const datagram = new DatagramEvent(uint8Array, session, early);
    session.dispatchEvent(datagram);
  },
  onSessionDatagramStatus(id, status) {
    const session = this[kOwner];
    // The event is ignored if the session has been destroyed.
    if (session.destroyed) return;
    const datagramStatus = new DatagramStatusEvent(id, status, session);
    session.dispatchEvent(datagramStatus);
  },
  onSessionHandshake(sni, alpn, cipher, cipherVersion,
                     validationErrorReason,
                     validationErrorCode,
                     earlyDataAccepted) {
    const session = this[kOwner];
    // The event is ignored if the session has been destroyed.
    if (session.destroyed) return;
    const handshake =
      new HandshakeEvent(sni, alpn, cipher, cipherVersion,
                         validationErrorReason, validationErrorCode,
                         earlyDataAccepted, session);
    session.dispatchEvent(handshake);
  },
  onSessionPathValidation(result,
                          newLocalAddress,
                          newRemoteAddress,
                          oldLocalAddress,
                          oldRemoteAddress,
                          preferredAddress) {
    const session = this[kOwner];
    // The event is ignored if the session has been destroyed.
    if (session.destroyed) return;
    const pathValidation = new PathValidationEvent(result,
                                                   newLocalAddress,
                                                   newRemoteAddress,
                                                   oldLocalAddress,
                                                   oldRemoteAddress,
                                                   preferredAddress,
                                                   session);
    session.dispatchEvent(pathValidation);
  },
  onSessionTicket(ticket) {
    const session = this[kOwner];
    // The event is ignored if the session has been destroyed.
    if (session.destroyed) return;
    const sessionTicket = new SessionTicketEvent(ticket, session);
    session.dispatchEvent(sessionTicket);
  },
  onSessionVersionNegotiation(version,
                              requestedVersions,
                              supportedVersions) {
    const session = this[kOwner];
    // The event is ignored if the session has been destroyed.
    if (session.destroyed) return;
    const versionNegotiation =
      new VersionNegotiationEvent(version,
                                  requestedVersions,
                                  supportedVersions,
                                  session);
    session.dispatchEvent(versionNegotiation);
    // Note that immediately following a version negotiation event, the
    // session will be destroyed.
  },
  onStreamCreated(stream, direction) {
    const session = this[kOwner];
    // The event is ignored if the session has been destroyed.
    if (session.destroyed) {
      stream.destroy();
      return;
    };
    session[kNewStream](stream, direction);
  },
  onStreamBlocked() {},
  onStreamClose(error) {},
  onStreamReset(error) {},
  onStreamHeaders(headers, kind) {},
  onStreamTrailers() {},
});

/**
 * @param {string} policy
 * @returns {number}
 */
function getPreferredAddressPolicy(policy) {
  if (policy === 'use') return PREFERRED_ADDRESS_USE;
  if (policy === 'ignore') return PREFERRED_ADDRESS_IGNORE;
  if (policy === 'default' || policy === undefined)
    return DEFAULT_PREFERRED_ADDRESS_POLICY;
  throw new ERR_INVALID_ARG_VALUE('options.preferredAddressPolicy', policy);
}

/**
 * @param {TlsOptions} tls
 */
function processTlsOptions(tls) {
  validateObject(tls, 'options.tls');
  const {
    sni,
    alpn,
    ciphers,
    groups,
    keylog,
    verifyClient,
    tlsTrace,
    verifyPrivateKey,
    keys,
    certs,
    ca,
    crl,
  } = tls;

  const keyHandles = [];
  if (keys !== undefined) {
    const keyInputs = ArrayIsArray(keys) ? keys : [keys];
    for (const key of keyInputs) {
      if (isKeyObject(key)) {
        if (key.type !== 'private') {
          throw new ERR_INVALID_ARG_VALUE('options.tls.keys', key, 'must be a private key');
        }
        keyHandles.push(key[kKeyObjectHandle]);
        continue;
      } else if (isCryptoKey(key)) {
        if (key.type !== 'private') {
          throw new ERR_INVALID_ARG_VALUE('options.tls.keys', key, 'must be a private key');
        }
        keyHandles.push(key[kKeyObjectInner][kKeyObjectHandle]);
        continue;
      } else {
        throw new ERR_INVALID_ARG_TYPE('options.tls.keys', ['KeyObject', 'CryptoKey'], key);
      }
    }
  }

  return {
    sni,
    alpn,
    ciphers,
    groups,
    keylog,
    verifyClient,
    tlsTrace,
    verifyPrivateKey,
    keys: keyHandles,
    certs,
    ca,
    crl,
  };
}

class StreamStats {
  constructor() {
    throw new ERR_INVALID_STATE('StreamStats');
  }

  get createdAt() {
    return this[kHandle][IDX_STATS_STREAM_CREATED_AT];
  }

  get receivedAt() {
    return this[kHandle][IDX_STATS_STREAM_RECEIVED_AT];
  }

  get ackedAt() {
    return this[kHandle][IDX_STATS_STREAM_ACKED_AT];
  }

  get closingAt() {
    return this[kHandle][IDX_STATS_STREAM_CLOSING_AT];
  }

  get destroyedAt() {
    return this[kHandle][IDX_STATS_STREAM_DESTROYED_AT];
  }

  get bytesReceived() {
    return this[kHandle][IDX_STATS_STREAM_BYTES_RECEIVED];
  }

  get bytesSent() {
    return this[kHandle][IDX_STATS_STREAM_BYTES_SENT];
  }

  get maxOffset() {
    return this[kHandle][IDX_STATS_STREAM_MAX_OFFSET];
  }

  get maxOffsetAcknowledged() {
    return this[kHandle][IDX_STATS_STREAM_MAX_OFFSET_ACK];
  }

  get maxOffsetReceived() {
    return this[kHandle][IDX_STATS_STREAM_MAX_OFFSET_RECV];
  }

  get finalSize() {
    return this[kHandle][IDX_STATS_STREAM_FINAL_SIZE];
  }

  toJSON() {
    return {
      createdAt: `${this.createdAt}`,
      receivedAt: `${this.receivedAt}`,
      ackedAt: `${this.ackedAt}`,
      closingAt: `${this.closingAt}`,
      destroyedAt: `${this.destroyedAt}`,
      bytesReceived: `${this.bytesReceived}`,
      bytesSent: `${this.bytesSent}`,
      maxOffset: `${this.maxOffset}`,
      maxOffsetAcknowledged: `${this.maxOffsetAcknowledged}`,
      maxOffsetReceived: `${this.maxOffsetReceived}`,
      finalSize: `${this.finalSize}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `StreamStats ${inspect({
      createdAt: this.createdAt,
      receivedAt: this.receivedAt,
      ackedAt: this.ackedAt,
      closingAt: this.closingAt,
      destroyedAt: this.destroyedAt,
      bytesReceived: this.bytesReceived,
      bytesSent: this.bytesSent,
      maxOffset: this.maxOffset,
      maxOffsetAcknowledged: this.maxOffsetAcknowledged,
      maxOffsetReceived: this.maxOffsetReceived,
      finalSize: this.finalSize,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the stats into a new BigUint64Array since the underlying
    // buffer will be destroyed.
    this[kHandle] = new BigUint64Array(this[kHandle]);
  }
}
ObjectDefineProperties(StreamStats.prototype, {
  createdAt: { __proto__: null, enumerable: true },
  receivedAt: { __proto__: null, enumerable: true },
  ackedAt: { __proto__: null, enumerable: true },
  closingAt: { __proto__: null, enumerable: true },
  destroyedAt: { __proto__: null, enumerable: true },
  bytesReceived: { __proto__: null, enumerable: true },
  bytesSent: { __proto__: null, enumerable: true },
  maxOffset: { __proto__: null, enumerable: true },
  maxOffsetAcknowledged: { __proto__: null, enumerable: true },
  maxOffsetReceived: { __proto__: null, enumerable: true },
  finalSize: { __proto__: null, enumerable: true },
});

/**
 * @param {ArrayBuffer} buffer
 * @returns {StreamStats}
 */
function createStreamStats(buffer) {
  return ReflectConstruct(function() {
    this[kHandle] = new BigUint64Array(buffer);
  }, [], StreamStats);
}

class StreamState {
  constructor() {
    throw new ERR_INVALID_STATE('StreamState');
  }

  /** @type {bigint} */
  get id() {
    return this[kHandle].getBigInt64(IDX_STATE_STREAM_ID);
  }

  /** @type {boolean} */
  get finSent() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_FIN_SENT);
  }

  /** @type {boolean} */
  get finReceived() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_FIN_RECEIVED);
  }

  /** @type {boolean} */
  get readEnded() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_READ_ENDED);
  }

  /** @type {boolean} */
  get writeEnded() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_WRITE_ENDED);
  }

  /** @type {boolean} */
  get destroyed() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_DESTROYED);
  }

  /** @type {boolean} */
  get paused() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_PAUSED);
  }

  /** @type {boolean} */
  get reset() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_RESET);
  }

  /** @type {boolean} */
  get hasReader() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_HAS_READER);
  }

  /** @type {boolean} */
  get wantsBlock() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_WANTS_BLOCK);
  }

  /** @type {boolean} */
  set wantsBlock(val) {
    this[kHandle].setUint8(IDX_STATE_STREAM_WANTS_BLOCK, val ? 1 : 0);
  }

  /** @type {boolean} */
  get wantsHeaders() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_WANTS_HEADERS);
  }

  /** @type {boolean} */
  set wantsHeaders(val) {
    this[kHandle].setUint8(IDX_STATE_STREAM_WANTS_HEADERS, val ? 1 : 0);
  }

  /** @type {boolean} */
  get wantsReset() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_WANTS_RESET);
  }

  /** @type {boolean} */
  set wantsReset(val) {
    this[kHandle].setUint8(IDX_STATE_STREAM_WANTS_RESET, val ? 1 : 0);
  }

  /** @type {boolean} */
  get wantsTrailers() {
    return !!this[kHandle].getUint8(IDX_STATE_STREAM_WANTS_TRAILERS);
  }

  /** @type {boolean} */
  set wantsTrailers(val) {
    this[kHandle].setUint8(IDX_STATE_STREAM_WANTS_TRAILERS, val ? 1 : 0);
  }

  toJSON() {
    return {
      id: `${this.id}`,
      finSent: this.finSent,
      finReceived: this.finReceived,
      readEnded: this.readEnded,
      writeEnded: this.writeEnded,
      destroyed: this.destroyed,
      paused: this.paused,
      reset: this.reset,
      hasReader: this.hasReader,
      wantsBlock: this.wantsBlock,
      wantsHeaders: this.wantsHeaders,
      wantsReset: this.wantsReset,
      wantsTrailers: this.wantsTrailers,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `StreamState ${inspect({
      id: this.id,
      finSent: this.finSent,
      finReceived: this.finReceived,
      readEnded: this.readEnded,
      writeEnded: this.writeEnded,
      destroyed: this.destroyed,
      paused: this.paused,
      reset: this.reset,
      hasReader: this.hasReader,
      wantsBlock: this.wantsBlock,
      wantsHeaders: this.wantsHeaders,
      wantsReset: this.wantsReset,
      wantsTrailers: this.wantsTrailers,
    }, opts)}`;
  }
}
ObjectDefineProperties(StreamState.prototype, {
  id: { __proto__: null, enumerable: true },
  finSent: { __proto__: null, enumerable: true },
  finReceived: { __proto__: null, enumerable: true },
  readEnded: { __proto__: null, enumerable: true },
  writeEnded: { __proto__: null, enumerable: true },
  destroyed: { __proto__: null, enumerable: true },
  paused: { __proto__: null, enumerable: true },
  reset: { __proto__: null, enumerable: true },
  hasReader: { __proto__: null, enumerable: true },
  wantsBlock: { __proto__: null, enumerable: true },
  wantsHeaders: { __proto__: null, enumerable: true },
  wantsReset: { __proto__: null, enumerable: true },
  wantsTrailers: { __proto__: null, enumerable: true },
});

/**
 * @param {ArrayBuffer} buffer
 * @returns {StreamState}
 */
function createStreamState(buffer) {
  return ReflectConstruct(function() {
    this[kHandle] = new DataView(buffer);
  }, [], StreamState);
}

class Stream extends EventTarget {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR('Stream');
  }

  /** @type {StreamStats} */
  get stats() { return this[kStats]; }

  /** @type {StreamState} */
  get state() { return this[kState]; }

  /** @type {Session} */
  get session() { return this[kSession]; }

  /** @type {bigint} */
  get id() { return this[kState].id; }
}
ObjectDefineProperties(Stream.prototype, {
  stats: { __proto__: null, enumerable: true },
  state: { __proto__: null, enumerable: true },
  session: { __proto__: null, enumerable: true },
  id: { __proto__: null, enumerable: true },
});

class BidirectionalStream extends Stream {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR('BidirectionalStream');
  }

  /** @type {ReadableStream} */
  get readable() { return this[kReadable]; }

  /** @type {WritableStream} */
  get writable() { return this[kWritable]; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `BidirectionalStream ${inspect({
      stats: this.stats,
      state: this.state,
      session: this.session,
      readable: this.readable,
      writable: this.writable,
    }, opts)}`;
  }
}
ObjectDefineProperties(BidirectionalStream.prototype, {
  readable: { __proto__: null, enumerable: true },
  writable: { __proto__: null, enumerable: true },
});

class UnidirectionalInboundStream extends Stream {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR('UnidirectionalInboundStream');
  }

  /** @type {ReadableStream} */
  get readable() { return this[kReadable]; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `UnidirectionalInboundStream ${inspect({
      stats: this.stats,
      state: this.state,
      session: this.session,
      readable: this.readable,
    }, opts)}`;
  }
}
ObjectDefineProperties(UnidirectionalInboundStream.prototype, {
  readable: { __proto__: null, enumerable: true },
});

class UnidirectionalOutboundStream extends Stream {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR('UnidirectionalOutboundStream');
  }

  /** @type {WritableStream} */
  get writable() { return this[kWritable]; }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `UnidirectionalOutboundStream ${inspect({
      stats: this.stats,
      state: this.state,
      session: this.session,
      writable: this.writable,
    }, opts)}`;
  }
}
ObjectDefineProperties(UnidirectionalOutboundStream.prototype, {
  writable: { __proto__: null, enumerable: true },
});

/**
 * @param {object} handle
 * @returns {BidirectionalStream}
 */
function createBidirectionalStream(handle, session) {
  const instance = ReflectConstruct(EventTarget, [], BidirectionalStream);
  instance[kHandle] = handle;
  instance[kStats] = createStreamStats(handle.stats);
  instance[kState] = createStreamState(handle.state);
  instance[kSession] = session;
  instance[kReadable] = new ReadableStream();
  instance[kWritable] = new WritableStream();
  handle[kOwner] = instance;
  return instance;
}

/**
 * @param {object} handle
 * @returns {UnidirectionalInboundStream}
 */
function createUnidirectionalInboundStream(handle, session) {
  const instance = ReflectConstruct(EventTarget, [], UnidirectionalInboundStream);
  instance[kHandle] = handle;
  instance[kStats] = createStreamStats(handle.stats);
  instance[kState] = createStreamState(handle.state);
  instance[kSession] = session;
  instance[kReadable] = new ReadableStream();
  handle[kOwner] = instance;
  return instance;
}

/**
 * @param {object} handle
 * @returns {UnidirectionalOutboundStream}
 */
function createUnidirectionalOutboundStream(handle, session) {
  const instance = ReflectConstruct(EventTarget, [], UnidirectionalOutboundStream);
  instance[kHandle] = handle;
  instance[kStats] = createStreamStats(handle.stats);
  instance[kState] = createStreamState(handle.state);
  instance[kSession] = session;
  instance[kWritable] = new WritableStream();
  handle[kOwner] = instance;
  return instance;
}

class SessionStats {
  constructor() {
    throw new ERR_INVALID_STATE('SessionStats');
  }

  /** @type {bigint} */
  get createdAt() {
    return this[kHandle][IDX_STATS_SESSION_CREATED_AT];
  }

  /** @type {bigint} */
  get closingAt() {
    return this[kHandle][IDX_STATS_SESSION_CLOSING_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    return this[kHandle][IDX_STATS_SESSION_DESTROYED_AT];
  }

  /** @type {bigint} */
  get handshakeCompletedAt() {
    return this[kHandle][IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT];
  }

  /** @type {bigint} */
  get handshakeConfirmedAt() {
    return this[kHandle][IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT];
  }

  /** @type {bigint} */
  get gracefulClosingAt() {
    return this[kHandle][IDX_STATS_SESSION_GRACEFUL_CLOSING_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    return this[kHandle][IDX_STATS_SESSION_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bytesSent() {
    return this[kHandle][IDX_STATS_SESSION_BYTES_SENT];
  }

  /** @type {bigint} */
  get bidiInStreamCount() {
    return this[kHandle][IDX_STATS_SESSION_BIDI_IN_STREAM_COUNT];
  }

  /** @type {bigint} */
  get bidiOutStreamCount() {
    return this[kHandle][IDX_STATS_SESSION_BIDI_OUT_STREAM_COUNT];
  }

  /** @type {bigint} */
  get uniInStreamCount() {
    return this[kHandle][IDX_STATS_SESSION_UNI_IN_STREAM_COUNT];
  }

  /** @type {bigint} */
  get uniOutStreamCount() {
    return this[kHandle][IDX_STATS_SESSION_UNI_OUT_STREAM_COUNT];
  }

  /** @type {bigint} */
  get lossRetransmitCount() {
    return this[kHandle][IDX_STATS_SESSION_LOSS_RETRANSMIT_COUNT];
  }

  /** @type {bigint} */
  get maxBytesInFlights() {
    return this[kHandle][IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT];
  }

  /** @type {bigint} */
  get bytesInFlight() {
    return this[kHandle][IDX_STATS_SESSION_BYTES_IN_FLIGHT];
  }

  /** @type {bigint} */
  get blockCount() {
    return this[kHandle][IDX_STATS_SESSION_BLOCK_COUNT];
  }

  /** @type {bigint} */
  get cwnd() {
    return this[kHandle][IDX_STATS_SESSION_CWND];
  }

  /** @type {bigint} */
  get latestRtt() {
    return this[kHandle][IDX_STATS_SESSION_LATEST_RTT];
  }

  /** @type {bigint} */
  get minRtt() {
    return this[kHandle][IDX_STATS_SESSION_MIN_RTT];
  }

  /** @type {bigint} */
  get rttVar() {
    return this[kHandle][IDX_STATS_SESSION_RTTVAR];
  }

  /** @type {bigint} */
  get smoothedRtt() {
    return this[kHandle][IDX_STATS_SESSION_SMOOTHED_RTT];
  }

  /** @type {bigint} */
  get ssthresh() {
    return this[kHandle][IDX_STATS_SESSION_SSTHRESH];
  }

  /** @type {bigint} */
  get datagramsReceived() {
    return this[kHandle][IDX_STATS_SESSION_DATAGRAMS_RECEIVED];
  }

  /** @type {bigint} */
  get datagramsSent() {
    return this[kHandle][IDX_STATS_SESSION_DATAGRAMS_SENT];
  }

  /** @type {bigint} */
  get datagramsAcknowledged() {
    return this[kHandle][IDX_STATS_SESSION_DATAGRAMS_ACKNOWLEDGED];
  }

  /** @type {bigint} */
  get datagramsLost() {
    return this[kHandle][IDX_STATS_SESSION_DATAGRAMS_LOST];
  }

  toJSON() {
    return {
      createdAt: `${this.createdAt}`,
      closingAt: `${this.closingAt}`,
      destroyedAt: `${this.destroyedAt}`,
      handshakeCompletedAt: `${this.handshakeCompletedAt}`,
      handshakeConfirmedAt: `${this.handshakeConfirmedAt}`,
      gracefulClosingAt: `${this.gracefulClosingAt}`,
      bytesReceived: `${this.bytesReceived}`,
      bytesSent: `${this.bytesSent}`,
      bidiInStreamCount: `${this.bidiInStreamCount}`,
      bidiOutStreamCount: `${this.bidiOutStreamCount}`,
      uniInStreamCount: `${this.uniInStreamCount}`,
      uniOutStreamCount: `${this.uniOutStreamCount}`,
      lossRetransmitCount: `${this.lossRetransmitCount}`,
      maxBytesInFlights: `${this.maxBytesInFlights}`,
      bytesInFlight: `${this.bytesInFlight}`,
      blockCount: `${this.blockCount}`,
      cwnd: `${this.cwnd}`,
      latestRtt: `${this.latestRtt}`,
      minRtt: `${this.minRtt}`,
      rttVar: `${this.rttVar}`,
      smoothedRtt: `${this.smoothedRtt}`,
      ssthresh: `${this.ssthresh}`,
      datagramsReceived: `${this.datagramsReceived}`,
      datagramsSent: `${this.datagramsSent}`,
      datagramsAcknowledged: `${this.datagramsAcknowledged}`,
      datagramsLost: `${this.datagramsLost}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `SessionStats ${inspect({
      createdAt: this.createdAt,
      closingAt: this.closingAt,
      destroyedAt: this.destroyedAt,
      handshakeCompletedAt: this.handshakeCompletedAt,
      handshakeConfirmedAt: this.handshakeConfirmedAt,
      gracefulClosingAt: this.gracefulClosingAt,
      bytesReceived: this.bytesReceived,
      bytesSent: this.bytesSent,
      bidiInStreamCount: this.bidiInStreamCount,
      bidiOutStreamCount: this.bidiOutStreamCount,
      uniInStreamCount: this.uniInStreamCount,
      uniOutStreamCount: this.uniOutStreamCount,
      lossRetransmitCount: this.lossRetransmitCount,
      maxBytesInFlights: this.maxBytesInFlights,
      bytesInFlight: this.bytesInFlight,
      blockCount: this.blockCount,
      cwnd: this.cwnd,
      latestRtt: this.latestRtt,
      minRtt: this.minRtt,
      rttVar: this.rttVar,
      smoothedRtt: this.smoothedRtt,
      ssthresh: this.ssthresh,
      datagramsReceived: this.datagramsReceived,
      datagramsSent: this.datagramsSent,
      datagramsAcknowledged: this.datagramsAcknowledged,
      datagramsLost: this.datagramsLost,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the stats into a new BigUint64Array since the underlying
    // buffer will be destroyed.
    this[kHandle] = new BigUint64Array(this[kHandle]);
  }
}
ObjectDefineProperties(SessionStats.prototype, {
  createdAt: { __proto__: null, enumerable: true },
  closingAt: { __proto__: null, enumerable: true },
  destroyedAt: { __proto__: null, enumerable: true },
  handshakeCompletedAt: { __proto__: null, enumerable: true },
  handshakeConfirmedAt: { __proto__: null, enumerable: true },
  gracefulClosingAt: { __proto__: null, enumerable: true },
  bytesReceived: { __proto__: null, enumerable: true },
  bytesSent: { __proto__: null, enumerable: true },
  bidiInStreamCount: { __proto__: null, enumerable: true },
  bidiOutStreamCount: { __proto__: null, enumerable: true },
  uniInStreamCount: { __proto__: null, enumerable: true },
  uniOutStreamCount: { __proto__: null, enumerable: true },
  lossRetransmitCount: { __proto__: null, enumerable: true },
  maxBytesInFlights: { __proto__: null, enumerable: true },
  bytesInFlight: { __proto__: null, enumerable: true },
  blockCount: { __proto__: null, enumerable: true },
  cwnd: { __proto__: null, enumerable: true },
  latestRtt: { __proto__: null, enumerable: true },
  minRtt: { __proto__: null, enumerable: true },
  rttVar: { __proto__: null, enumerable: true },
  smoothedRtt: { __proto__: null, enumerable: true },
  ssthresh: { __proto__: null, enumerable: true },
  datagramsReceived: { __proto__: null, enumerable: true },
  datagramsSent: { __proto__: null, enumerable: true },
  datagramsAcknowledged: { __proto__: null, enumerable: true },
  datagramsLost: { __proto__: null, enumerable: true },
});

/**
 *
 * @param {ArrayBuffer} buffer
 * @returns {SessionStats}
 */
function createSessionStats(buffer) {
  return ReflectConstruct(function() {
    this[kHandle] = new BigUint64Array(buffer);
  }, [], SessionStats);
}

class SessionState {
  constructor() {
    throw new ERR_INVALID_STATE('SessionState');
  }

  /** @type {boolean} */
  get hasPathValidationListener() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_PATH_VALIDATION);
  }

  set hasPathValidationListener(val) {
    this[kHandle].setUint8(IDX_STATE_SESSION_PATH_VALIDATION, val ? 1 : 0);
  }

  /** @type {boolean} */
  get hasVersionNegotiationListener() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_VERSION_NEGOTIATION);
  }

  set hasVersionNegotiationListener(val) {
    this[kHandle].setUint8(IDX_STATE_SESSION_VERSION_NEGOTIATION, val ? 1 : 0);
  }

  /** @type {boolean} */
  get hasDatagramListener() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_DATAGRAM);
  }

  set hasDatagramListener(val) {
    this[kHandle].setUint8(IDX_STATE_SESSION_DATAGRAM, val ? 1 : 0);
  }

  /** @type {boolean} */
  get hasSessionTicketListener() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_SESSION_TICKET);
  }

  set hasSessionTicketListener(val) {
    this[kHandle].setUint8(IDX_STATE_SESSION_SESSION_TICKET, val ? 1 : 0);
  }

  /** @type {boolean} */
  get isClosing() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_CLOSING);
  }

  /** @type {boolean} */
  get isGracefulClose() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_GRACEFUL_CLOSE);
  }

  /** @type {boolean} */
  get isSilentClose() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_SILENT_CLOSE);
  }

  /** @type {boolean} */
  get isStatelessReset() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_STATELESS_RESET);
  }

  /** @type {boolean} */
  get isDestroyed() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_DESTROYED);
  }

  /** @type {boolean} */
  get isHandshakeCompleted() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_HANDSHAKE_COMPLETED);
  }

  /** @type {boolean} */
  get isHandshakeConfirmed() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_HANDSHAKE_CONFIRMED);
  }

  /** @type {boolean} */
  get isStreamOpenAllowed() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_STREAM_OPEN_ALLOWED);
  }

  /** @type {boolean} */
  get isPrioritySupported() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_PRIORITY_SUPPORTED);
  }

  /** @type {boolean} */
  get isWrapped() {
    return !!this[kHandle].getUint8(IDX_STATE_SESSION_WRAPPED);
  }

  /** @type {bigint} */
  get lastDatagramId() {
    return this[kHandle].getBigUint64(IDX_STATE_SESSION_LAST_DATAGRAM_ID);
  }

  toJSON() {
    return {
      hasPathValidationListener: this.hasPathValidationListener,
      hasVersionNegotiationListener: this.hasVersionNegotiationListener,
      hasDatagramListener: this.hasDatagramListener,
      hasSessionTicketListener: this.hasSessionTicketListener,
      isClosing: this.isClosing,
      isGracefulClose: this.isGracefulClose,
      isSilentClose: this.isSilentClose,
      isStatelessReset: this.isStatelessReset,
      isDestroyed: this.isDestroyed,
      isHandshakeCompleted: this.isHandshakeCompleted,
      isHandshakeConfirmed: this.isHandshakeConfirmed,
      isStreamOpenAllowed: this.isStreamOpenAllowed,
      isPrioritySupported: this.isPrioritySupported,
      isWrapped: this.isWrapped,
      lastDatagramId: `${this.lastDatagramId}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `SessionState ${inspect({
      hasPathValidationListener: this.hasPathValidationListener,
      hasVersionNegotiationListener: this.hasVersionNegotiationListener,
      hasDatagramListener: this.hasDatagramListener,
      hasSessionTicketListener: this.hasSessionTicketListener,
      isClosing: this.isClosing,
      isGracefulClose: this.isGracefulClose,
      isSilentClose: this.isSilentClose,
      isStatelessReset: this.isStatelessReset,
      isDestroyed: this.isDestroyed,
      isHandshakeCompleted: this.isHandshakeCompleted,
      isHandshakeConfirmed: this.isHandshakeConfirmed,
      isStreamOpenAllowed: this.isStreamOpenAllowed,
      isPrioritySupported: this.isPrioritySupported,
      isWrapped: this.isWrapped,
      lastDatagramId: this.lastDatagramId,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the state into a new DataView since the underlying
    // buffer will be destroyed.
    this[kHandle] = new DataView(this[kHandle]);
  }
}
ObjectDefineProperties(SessionState.prototype, {
  hasPathValidationListener: { __proto__: null, enumerable: true },
  hasVersionNegotiationListener: { __proto__: null, enumerable: true },
  hasDatagramListener: { __proto__: null, enumerable: true },
  hasSessionTicketListener: { __proto__: null, enumerable: true },
  isClosing: { __proto__: null, enumerable: true },
  isGracefulClose: { __proto__: null, enumerable: true },
  isSilentClose: { __proto__: null, enumerable: true },
  isStatelessReset: { __proto__: null, enumerable: true },
  isDestroyed: { __proto__: null, enumerable: true },
  isHandshakeCompleted: { __proto__: null, enumerable: true },
  isHandshakeConfirmed: { __proto__: null, enumerable: true },
  isStreamOpenAllowed: { __proto__: null, enumerable: true },
  isPrioritySupported: { __proto__: null, enumerable: true },
  isWrapped: { __proto__: null, enumerable: true },
  lastDatagramId: { __proto__: null, enumerable: true },
});

/**
 * @param {ArrayBuffer} buffer
 * @returns {SessionState}
 */
function createSessionState(buffer) {
  return ReflectConstruct(function() {
    this[kHandle] = new DataView(buffer);
  }, [], SessionState);

}

class Session extends EventTarget {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR('Session');
  }

  get #isClosedOrClosing() {
    return this[kHandle] === undefined || this[kIsPendingClose];
  }

  /** @type {SessionStats} */
  get stats() { return this[kStats]; }

  /** @type {SessionState} */
  get state() { return this[kState]; }

  /** @type {Endpoint} */
  get endpoint() { return this[kEndpoint]; }

  /** @type {Path} */
  get path() {
    if (this.#isClosedOrClosing) return undefined;
    if (this[kRemoteAddress] === undefined) {
      const addr = this[kHandle].getRemoteAddress();
      if (addr !== undefined) {
        this[kRemoteAddress] = new InternalSocketAddress(addr);
      }
    }
    return {
      local: this[kEndpoint].address,
      remote: this[kRemoteAddress],
    };
  }

  /**
   * @returns {BidirectionalStream}
   */
  openBidirectionalStream() {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    if (!this.state.isStreamOpenAllowed) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }
    const handle = this[kHandle].openStream(STREAM_DIRECTION_BIDIRECTIONAL);
    if (handle === undefined) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }
    const stream = createBidirectionalStream(handle, this);
    this[kStreams].push(stream);
    return stream;
  }

  /**
   * @returns {UnidirectionalInboundStream}
   */
  openUnidirectionalStream() {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    if (!this.state.isStreamOpenAllowed) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }
    const handle = this[kHandle].openStream(STREAM_DIRECTION_UNIDIRECTIONAL);
    if (handle === undefined) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }
    const stream = createUnidirectionalOutboundStream(handle, this);
    this[kStreams].push(stream);
    return stream;
  }

  /**
   * Initiate a key update.
   */
  updateKey() {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    this[kHandle].updateKey();
  }

  /**
   * Gracefully closes the session. Any streams created on the session will be
   * allowed to complete gracefully and any datagrams that have already been
   * queued for sending will be allowed to complete. Once all streams and been
   * completed and all datagrams have been sent, the session will be closed.
   * New streams will not be allowed to be created. The returns promise will
   * be resolved when the session closes, or will be rejected if the session
   * closes abruptly due to an error.
   * @returns {Promise<void>}
   */
  close() {
    if (!this.#isClosedOrClosing) {
      this[kIsPendingClose] = true;
      this[kHandle]?.gracefulClose();
    }
    return this.closed;
  }

  /**
   * A promise that is resolved when the session is closed, or is rejected if
   * the session is closed abruptly due to an error.
   * @type {Promise<void>}
   */
  get closed() { return this[kPendingClose].promise; }

  /** @type {boolean} */
  get destroyed() { return this[kHandle] === undefined; }

  /**
   * Forcefully closes the session abruptly without waiting for streams to be
   * completed naturally. Any streams that are still open will be immediately
   * destroyed and any queued datagrams will be dropped. If an error is given,
   * the closed promise will be rejected with that error. If no error is given,
   * the closed promise will be resolved.
   * @param {any} error
   */
  destroy(error) {
    // TODO(@jasnell): Implement.
  }

  /**
   * Send a datagram. The id of the sent datagram will be returned. The status
   * of the sent datagram will be reported via the datagram-status event if
   * possible.
   *
   * If a string is given it will be encoded as UTF-8.
   *
   * If an ArrayBufferView is given, the view will be copied.
   * @param {ArrayBufferView|string} datagram The datagram payload
   * @returns {bigint} The datagram ID
   */
  sendDatagram(datagram) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    if (typeof datagram === 'string') {
      datagram = Buffer.from(datagram, 'utf8');
    } else {
      if (!isArrayBufferView(datagram)) {
        throw new ERR_INVALID_ARG_TYPE('datagram',
                                       ['ArrayBufferView', 'string'],
                                       datagram);
      }
      datagram = new Uint8Array(datagram.buffer,
                                datagram.byteOffset,
                                datagram.byteLength);
    }
    return this[kHandle].sendDatagram(new Uint8Array(datagram));
  }

  [kNewListener](size, type, listener, once, capture, passive, weak) {
    super[kNewListener](size, type, listener, once, capture, passive, weak);
    if (type === 'path-validation') {
      this[kState].hasPathValidationListener = true;
    } else if (type === 'version-negotiation') {
      this[kState].hasVersionNegotiationListener = true;
    } else if (type === 'datagram') {
      this[kState].hasDatagramListener = true;
    } else if (type === 'session-ticket') {
      this[kState].hasSessionTicketListener = true;
    }
  }

  [kRemoveListener](size, type, listener, capture) {
    super[kRemoveListener](size, type, listener, capture);
    if (type === 'path-validation') {
      this[kState].hasPathValidationListener = size > 0;
    } else if (type === 'version-negotiation') {
      this[kState].hasVersionNegotiationListener = size > 0;
    } else if (type === 'datagram') {
      this[kState].hasDatagramListener = size > 0;
    } else if (type === 'session-ticket') {
      this[kState].hasSessionTicketListener = size > 0;
    }
  }

  [kNewStream](handle, direction) {
    const stream = (() => {
      if (direction === 0) {
        return createBidirectionalStream(handle, this);
      }
      return createUnidirectionalInboundStream(handle, this);
    })();
    this[kStreams].push(stream);
    const event = new StreamEvent(stream, this);
    this.dispatchEvent(event);
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `Session ${inspect({
      closed: this[kPendingClose].promise,
      closing: this[kIsPendingClose],
      destroyed: this.destroyed,
      endpoint: this.endpoint,
      path: this.path,
      state: this.state,
      stats: this.stats,
      streams: this[kStreams],
    }, opts)}`;
  }

  [kFinishClose](errorType, code, reason) {
    // TODO(@jasnell): Finish the implementation
  }
}
ObjectDefineProperties(Session.prototype, {
  closed: { __proto__: null, enumerable: true },
  destroyed: { __proto__: null, enumerable: true },
  endpoint: { __proto__: null, enumerable: true },
  localAddress: { __proto__: null, enumerable: true },
  remoteAddress: { __proto__: null, enumerable: true },
  state: { __proto__: null, enumerable: true },
  stats: { __proto__: null, enumerable: true },
});

/**
 * @param {object} handle
 * @returns {Session}
 */
function createSession(handle, endpoint) {
  const instance = ReflectConstruct(EventTarget, [], Session);
  instance[kHandle] = handle;
  instance[kStats] = createSessionStats(handle.stats);
  instance[kState] = createSessionState(handle.state);
  instance[kEndpoint] = endpoint;
  instance[kStreams] = [];
  instance[kRemoteAddress] = undefined;
  instance[kIsPendingClose] = false;
  instance[kPendingClose] = Promise.withResolvers(); // eslint-disable-line node-core/prefer-primordials
  handle[kOwner] = instance;
  return instance;
}

class EndpointStats {
  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR('EndpointStats');
  }

  /** @type {bigint} */
  get createdAt() {
    return this[kHandle][IDX_STATS_ENDPOINT_CREATED_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    return this[kHandle][IDX_STATS_ENDPOINT_DESTROYED_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    return this[kHandle][IDX_STATS_ENDPOINT_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bytesSent() {
    return this[kHandle][IDX_STATS_ENDPOINT_BYTES_SENT];
  }

  /** @type {bigint} */
  get packetsReceived() {
    return this[kHandle][IDX_STATS_ENDPOINT_PACKETS_RECEIVED];
  }

  /** @type {bigint} */
  get packetsSent() {
    return this[kHandle][IDX_STATS_ENDPOINT_PACKETS_SENT];
  }

  /** @type {bigint} */
  get serverSessions() {
    return this[kHandle][IDX_STATS_ENDPOINT_SERVER_SESSIONS];
  }

  /** @type {bigint} */
  get clientSessions() {
    return this[kHandle][IDX_STATS_ENDPOINT_CLIENT_SESSIONS];
  }

  /** @type {bigint} */
  get serverBusyCount() {
    return this[kHandle][IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT];
  }

  /** @type {bigint} */
  get retryCount() {
    return this[kHandle][IDX_STATS_ENDPOINT_RETRY_COUNT];
  }

  /** @type {bigint} */
  get versionNegotiationCount() {
    return this[kHandle][IDX_STATS_ENDPOINT_VERSION_NEGOTIATION_COUNT];
  }

  /** @type {bigint} */
  get statelessResetCount() {
    return this[kHandle][IDX_STATS_ENDPOINT_STATELESS_RESET_COUNT];
  }

  /** @type {bigint} */
  get immediateCloseCount() {
    return this[kHandle][IDX_STATS_ENDPOINT_IMMEDIATE_CLOSE_COUNT];
  }

  toJSON() {
    return {
      createdAt: `${this.createdAt}`,
      destroyedAt: `${this.destroyedAt}`,
      bytesReceived: `${this.bytesReceived}`,
      bytesSent: `${this.bytesSent}`,
      packetsReceived: `${this.packetsReceived}`,
      packetsSent: `${this.packetsSent}`,
      serverSessions: `${this.serverSessions}`,
      clientSessions: `${this.clientSessions}`,
      serverBusyCount: `${this.serverBusyCount}`,
      retryCount: `${this.retryCount}`,
      versionNegotiationCount: `${this.versionNegotiationCount}`,
      statelessResetCount: `${this.statelessResetCount}`,
      immediateCloseCount: `${this.immediateCloseCount}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `EndpointStats ${inspect({
      createdAt: this.createdAt,
      destroyedAt: this.destroyedAt,
      bytesReceived: this.bytesReceived,
      bytesSent: this.bytesSent,
      packetsReceived: this.packetsReceived,
      packetsSent: this.packetsSent,
      serverSessions: this.serverSessions,
      clientSessions: this.clientSessions,
      serverBusyCount: this.serverBusyCount,
      retryCount: this.retryCount,
      versionNegotiationCount: this.versionNegotiationCount,
      statelessResetCount: this.statelessResetCount,
      immediateCloseCount: this.immediateCloseCount,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the stats into a new BigUint64Array since the underlying
    // buffer will be destroyed.
    this[kHandle] = new BigUint64Array(this[kHandle]);
  }
}
ObjectDefineProperties(EndpointStats.prototype, {
  createdAt: { __proto__: null, enumerable: true },
  destroyedAt: { __proto__: null, enumerable: true },
  bytesReceived: { __proto__: null, enumerable: true },
  bytesSent: { __proto__: null, enumerable: true },
  packetsReceived: { __proto__: null, enumerable: true },
  packetsSent: { __proto__: null, enumerable: true },
  serverSessions: { __proto__: null, enumerable: true },
  clientSessions: { __proto__: null, enumerable: true },
  serverBusyCount: { __proto__: null, enumerable: true },
  retryCount: { __proto__: null, enumerable: true },
  versionNegotiationCount: { __proto__: null, enumerable: true },
  statelessResetCount: { __proto__: null, enumerable: true },
  immediateCloseCount: { __proto__: null, enumerable: true },
});

/**
 * @param {ArrayBuffer} buffer
 * @returns {EndpointStats}
 */
function createEndpointStats(buffer) {
  return ReflectConstruct(function() {
    this[kHandle] = new BigUint64Array(buffer);
  }, [], EndpointStats);
}

class EndpointState {
  constructor() {
    throw new ERR_INVALID_STATE('EndpointState');
  }

  /** @type {boolean} */
  get isBound() {
    return !!this[kHandle].getUint8(IDX_STATE_ENDPOINT_BOUND);
  }

  /** @type {boolean} */
  get isReceiving() {
    return !!this[kHandle].getUint8(IDX_STATE_ENDPOINT_RECEIVING);
  }

  /** @type {boolean} */
  get isListening() {
    return !!this[kHandle].getUint8(IDX_STATE_ENDPOINT_LISTENING);
  }

  /** @type {boolean} */
  get isClosing() {
    return !!this[kHandle].getUint8(IDX_STATE_ENDPOINT_CLOSING);
  }

  /** @type {boolean} */
  get isBusy() {
    return !!this[kHandle].getUint8(IDX_STATE_ENDPOINT_BUSY);
  }

  /**
   * The number of underlying callbacks that are pending. If the session
   * is closing, these are the number of callbacks that the session is
   * waiting on before it can be closed.
   * @type {bigint}
   */
  get pendingCallbacks() {
    return this[kHandle].getBigUint64(IDX_STATE_ENDPOINT_PENDING_CALLBACKS);
  }

  toJSON() {
    return {
      isBound: this.isBound,
      isReceiving: this.isReceiving,
      isListening: this.isListening,
      isClosing: this.isClosing,
      isBusy: this.isBusy,
      pendingCallbacks: `${this.pendingCallbacks}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `EndpointState ${inspect({
      isBound: this.isBound,
      isReceiving: this.isReceiving,
      isListening: this.isListening,
      isClosing: this.isClosing,
      isBusy: this.isBusy,
      pendingCallbacks: this.pendingCallbacks,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the state into a new DataView since the underlying
    // buffer will be destroyed.
    this[kHandle] = new DataView(this[kHandle]);
  }
}
ObjectDefineProperties(EndpointState.prototype, {
  isBound: { __proto__: null, enumerable: true },
  isReceiving: { __proto__: null, enumerable: true },
  isListening: { __proto__: null, enumerable: true },
  isClosing: { __proto__: null, enumerable: true },
  isBusy: { __proto__: null, enumerable: true },
  pendingCallbacks: { __proto__: null, enumerable: true },
});

/**
 * @param {ArrayBuffer} buffer
 * @returns {EndpointState}
 */
function createEndpointState(buffer) {
  return ReflectConstruct(function() {
    this[kHandle] = new DataView(buffer);
  }, [], EndpointState);
}

class Endpoint extends EventTarget {
  #address = undefined;
  #busy = false;
  #handle = undefined;
  #isPendingClose = false;
  #listening = false;
  #pendingClose = undefined;
  #pendingError = undefined;
  #sessions = [];
  #state = undefined;
  #stats = undefined;

  /**
   * @param {EndpointOptions} [options]
   */
  constructor(options = {}) {
    validateObject(options, 'options');

    let { address } = options;
    const {
      retryTokenExpiration,
      tokenExpiration,
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResetsPerHost,
      addressLRUSize,
      maxRetries,
      maxPayloadSize,
      unacknowledgedPacketThreshold,
      handshakeTimeout,
      maxStreamWindow,
      maxWindow,
      rxDiagnosticLoss,
      txDiagnosticLoss,
      udpReceiveBufferSize,
      udpSendBufferSize,
      udpTTL,
      noUdpPayloadSizeShaping,
      validateAddress,
      disableActiveMigration,
      ipv6Only,
      cc,
      resetTokenSecret,
      tokenSecret,
    } = options;

    // All of the other options will be validated internally by the C++ code
    if (address !== undefined && !SocketAddress.isSocketAddress(address)) {
      if (typeof address === 'object' && address !== null) {
        address = new SocketAddress(address);
      } else {
        throw new ERR_INVALID_ARG_TYPE('options.address', 'SocketAddress', address);
      }
    }

    super();
    this.#pendingClose = Promise.withResolvers();  // eslint-disable-line node-core/prefer-primordials
    this.#handle = new Endpoint_({
      address: address?.[kSocketAddressHandle],
      retryTokenExpiration,
      tokenExpiration,
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResetsPerHost,
      addressLRUSize,
      maxRetries,
      maxPayloadSize,
      unacknowledgedPacketThreshold,
      handshakeTimeout,
      maxStreamWindow,
      maxWindow,
      rxDiagnosticLoss,
      txDiagnosticLoss,
      udpReceiveBufferSize,
      udpSendBufferSize,
      udpTTL,
      noUdpPayloadSizeShaping,
      validateAddress,
      disableActiveMigration,
      ipv6Only,
      cc,
      resetTokenSecret,
      tokenSecret,
    });
    this.#handle[kOwner] = this;
    this.#stats = createEndpointStats(this.#handle.stats);
    this.#state = createEndpointState(this.#handle.state);
  }

  /** @type {EndpointStats} */
  get stats() { return this.#stats; }

  /** @type {EndpointState} */
  get state() { return this.#state; }

  get #isClosedOrClosing() {
    return this.#handle === undefined || this.#isPendingClose;
  }

  /**
   * When an endpoint is marked as busy, it will not accept new connections.
   * Existing connections will continue to work.
   * @type {boolean}
   */
  get busy() { return this.#busy; }

  /**
   * @type {boolean}
   */
  set busy(val) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Endpoint is closed');
    }
    // The val is allowed to be any truthy value
    val = !!val;
    // Non-op if there is no change
    if (val !== this.#busy) {
      this.#busy = val;
      this.#handle.markBusy(this.#busy);
    }
  }

  /**
   * The local address the endpoint is bound to (if any)
   * @type {SocketAddress|undefined}
   */
  get address() {
    if (this.#isClosedOrClosing) return undefined;
    if (this.#address === undefined) {
      const addr = this.#handle.address();
      if (addr !== undefined) this.#address = new InternalSocketAddress(addr);
    }
    return this.#address;
  }

  /**
   * Configures the endpoint to listen for incoming connections.
   * @param {SessionOptions} [options]
   */
  listen(options = {}) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Endpoint is closed');
    }
    if (this.#listening) {
      throw new ERR_INVALID_STATE('Endpoint is already listening');
    }

    validateObject(options, 'options');

    const {
      version,
      minVersion,
      preferredAddressPolicy = 'default',
      application = {},
      transportParams = {},
      tls = {},
      qlog = false,
    } = options;

    this.#handle.listen({
      version,
      minVersion,
      preferredAddressPolicy: getPreferredAddressPolicy(preferredAddressPolicy),
      application,
      transportParams,
      tls: processTlsOptions(tls),
      qlog,
    });
    this.#listening = true;
  }

  /**
   * Initiates a session with a remote endpoint.
   * @param {SocketAddress} address
   * @param {SessionOptions} [options]
   * @returns {Session}
   */
  connect(address, options = {}) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Endpoint is closed');
    }
    if (this.#busy) {
      throw new ERR_INVALID_STATE('Endpoint is busy');
    }

    if (address === undefined || !SocketAddress.isSocketAddress(address)) {
      if (typeof address === 'object' && address !== null) {
        address = new SocketAddress(address);
      } else {
        throw new ERR_INVALID_ARG_TYPE('address', 'SocketAddress', address);
      }
    }

    validateObject(options, 'options');
    const {
      version,
      minVersion,
      preferredAddressPolicy = 'default',
      application = {},
      transportParams = {},
      tls = {},
      qlog = false,
      sessionTicket,
    } = options;

    const handle = this.#handle.connect(address[kSocketAddressHandle], {
      version,
      minVersion,
      preferredAddressPolicy: getPreferredAddressPolicy(preferredAddressPolicy),
      application,
      transportParams,
      tls: processTlsOptions(tls),
      qlog,
    }, sessionTicket);

    if (handle === undefined) {
      throw new ERR_QUIC_CONNECTION_FAILED();
    }
    const session = createSession(handle, this);
    this.#sessions.push(session);
    return session;
  }

  /**
   * Gracefully closes the endpoint. Any existing sessions will be permitted
   * to gracefully, afterwhich the endpoint will be closed. New sessions will
   * not be accepted or created. The returned promise will be resolved when
   * closing is complete, or will be rejected if the endpoint is closed abruptly
   * due to an error.
   * @returns {Promise<void>}
   */
  close() {
    if (!this.#isClosedOrClosing) {
      this.#isPendingClose = true;
      this.#handle?.closeGracefully();
    }
    return this.closed;
  }

  /**
   * Returns a promise that is resolved when the endpoint is closed or rejects
   * if the endpoint is closed abruptly due to an error. The closed property
   * is set to the same promise that is returned by the close() method.
   * @type {Promise<void>}
   */
  get closed() { return this.#pendingClose.promise; }

  /** @type {boolean} */
  get destroyed() { return this.#handle === undefined; }

  /**
   * Forcefully terminates the endpoint by immediately destroying all sessions
   * after calling close. If an error is given, the closed promise will be
   * rejected with that error. If no error is given, the closed promise will
   * be resolved.
   * @param {any} error
   */
  destroy(error) {
    if (!this.#isClosedOrClosing) {
      // Start closing the endpoint.
      this.#pendingError = error;
      // Trigger a graceful close of the endpoint that'll ensure that the
      // endpoint is closed down after all sessions are closed...
      this.close();
    }
    // Now, force all sessions to be abruptly closed...
    for (const session of this.#sessions) {
      session.destroy(error);
    }
  }

  ref() {
    if (this.#handle !== undefined) this.#handle.ref(true);
  }

  unref() {
    if (this.#handle !== undefined) this.#handle.ref(false);
  }

  [kFinishClose](context, status) {
    if (this.#handle === undefined) return;
    this.#isPendingClose = false;
    this.#address = undefined;
    this.#busy = false;
    this.#listening = false;
    this.#isPendingClose = false;
    this.#stats[kFinishClose]();
    this.#state[kFinishClose]();
    this.#sessions = undefined;

    switch (context) {
      case kCloseContextClose: {
        if (this.#pendingError !== undefined) {
          this.#pendingClose.reject(this.#pendingError);
        } else {
          this.#pendingClose.resolve();
        }
        break;
      }
      case kCloseContextBindFailure: {
        this.#pendingClose.reject(
          new ERR_QUIC_ENDPOINT_CLOSED('Bind failure', status));
        break;
      }
      case kCloseContextListenFailure: {
        this.#pendingClose.reject(
          new ERR_QUIC_ENDPOINT_CLOSED('Listen failure', status));
        break;
      }
      case kCloseContextReceiveFailure: {
        this.#pendingClose.reject(
          new ERR_QUIC_ENDPOINT_CLOSED('Receive failure', status));
        break;
      }
      case kCloseContextSendFailure: {
        this.#pendingClose.reject(
          new ERR_QUIC_ENDPOINT_CLOSED('Send failure', status));
        break;
      }
      case kCloseContextStartFailure: {
        this.#pendingClose.reject(
          new ERR_QUIC_ENDPOINT_CLOSED('Start failure', status));
        break;
      }
    }

    this.#pendingError = undefined;
    this.#pendingClose.resolve = undefined;
    this.#pendingClose.reject = undefined;
    this.#handle = undefined;
  }

  [kNewSession](handle) {
    const session = createSession(handle, this);
    this.#sessions.push(session);
    this.dispatchEvent(new SessionEvent(session, this));
  }

  [SymbolAsyncDispose]() { return this.close(); }
  [SymbolDispose]() { this.close(); }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `Endpoint ${inspect({
      address: this.address,
      busy: this.#busy,
      closed: this.#pendingClose.promise,
      closing: this.#isPendingClose,
      destroyed: this.destroyed,
      listening: this.#listening,
      sessions: this.#sessions,
      stats: this.stats,
      state: this.state,
    }, opts)}`;
  }
};

ObjectDefineProperties(Endpoint.prototype, {
  address: { __proto__: null, enumerable: true },
  busy: { __proto__: null, enumerable: true },
  closed: { __proto__: null, enumerable: true },
  destroyed: { __proto__: null, enumerable: true },
  state: { __proto__: null, enumerable: true },
  stats: { __proto__: null, enumerable: true },
});
ObjectDefineProperties(Endpoint, {
  CC_ALGO_RENO: {
    __proto__: null,
    value: CC_ALGO_RENO,
    writable: false,
    configurable: false,
    enumerable: true,
  },
  CC_ALGO_CUBIC: {
    __proto__: null,
    value: CC_ALGO_CUBIC,
    writable: false,
    configurable: false,
    enumerable: true,
  },
  CC_ALGO_BBR: {
    __proto__: null,
    value: CC_ALGO_BBR,
    writable: false,
    configurable: false,
    enumerable: true,
  },
  CC_ALGO_RENO_STR: {
    __proto__: null,
    value: CC_ALGO_RENO_STR,
    writable: false,
    configurable: false,
    enumerable: true,
  },
  CC_ALGO_CUBIC_STR: {
    __proto__: null,
    value: CC_ALGO_CUBIC_STR,
    writable: false,
    configurable: false,
    enumerable: true,
  },
  CC_ALGO_BBR_STR: {
    __proto__: null,
    value: CC_ALGO_BBR_STR,
    writable: false,
    configurable: false,
    enumerable: true,
  },
});
ObjectDefineProperties(Session, {
  DEFAULT_CIPHERS: {
    __proto__: null,
    value: DEFAULT_CIPHERS,
    writable: false,
    configurable: false,
    enumerable: true,
  },
  DEFAULT_GROUPS: {
    __proto__: null,
    value: DEFAULT_GROUPS,
    writable: false,
    configurable: false,
    enumerable: true,
  },
});

module.exports = {
  Endpoint,
  EndpointStats,
  EndpointState,
  Session,
  SessionStats,
  SessionState,
  Stream,
  StreamStats,
  StreamState,
  BidirectionalStream,
  UnidirectionalInboundStream,
  UnidirectionalOutboundStream,
  DatagramEvent,
  DatagramStatusEvent,
  HandshakeEvent,
  PathValidationEvent,
  SessionEvent,
  SessionTicketEvent,
  StreamEvent,
  VersionNegotiationEvent,
};
