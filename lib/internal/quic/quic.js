'use strict';

/* eslint-disable no-use-before-define */

const {
  ArrayIsArray,
  BigInt,
  BigUint64Array,
  Boolean,
  DataView,
  NumberIsInteger,
  ObjectCreate,
  ObjectDefineProperties,
  Symbol,
} = primordials;

const {
  setCallbacks,
  createEndpoint,
  EndpointOptions: EndpointOptions_,
  SessionOptions: SessionOptions_,
  ArrayBufferViewSource,
  StreamSource,
  StreamBaseSource,
  BlobSource,

  QUIC_CC_ALGO_CUBIC,
  QUIC_CC_ALGO_RENO,
  QUIC_CC_ALGO_BBR,
  QUIC_CC_ALGO_BBR2,
  QUIC_MAX_CIDLEN,

  QUIC_ENDPOINT_CLOSE_CONTEXT_CLOSE,
  QUIC_ENDPOINT_CLOSE_CONTEXT_BIND_FAILURE,
  QUIC_ENDPOINT_CLOSE_CONTEXT_START_FAILURE,
  QUIC_ENDPOINT_CLOSE_CONTEXT_RECEIVE_FAILURE,
  QUIC_ENDPOINT_CLOSE_CONTEXT_SEND_FAILURE,
  QUIC_ENDPOINT_CLOSE_CONTEXT_LISTEN_FAILURE,

  QUIC_ERROR_TYPE_TRANSPORT,
  QUIC_ERROR_TYPE_APPLICATION,
  QUIC_ERROR_TYPE_VERSION_NEGOTIATION,
  QUIC_ERROR_TYPE_IDLE_CLOSE,

  QUIC_ERR_NO_ERROR,
  QUIC_ERR_INTERNAL_ERROR,
  QUIC_ERR_CONNECTION_REFUSED,
  QUIC_ERR_FLOW_CONTROL_ERROR,
  QUIC_ERR_STREAM_LIMIT_ERROR,
  QUIC_ERR_STREAM_STATE_ERROR,
  QUIC_ERR_FINAL_SIZE_ERROR,
  QUIC_ERR_FRAME_ENCODING_ERROR,
  QUIC_ERR_TRANSPORT_PARAMETER_ERROR,
  QUIC_ERR_CONNECTION_ID_LIMIT_ERROR,
  QUIC_ERR_PROTOCOL_VIOLATION,
  QUIC_ERR_INVALID_TOKEN,
  QUIC_ERR_APPLICATION_ERROR,
  QUIC_ERR_CRYPTO_BUFFER_EXCEEDED,
  QUIC_ERR_KEY_UPDATE_ERROR,
  QUIC_ERR_AEAD_LIMIT_REACHED,
  QUIC_ERR_NO_VIABLE_PATH,
  QUIC_ERR_CRYPTO_ERROR,
  QUIC_ERR_VERSION_NEGOTIATION_ERROR_DRAFT,

  QUIC_ERR_H3_GENERAL_PROTOCOL_ERROR,
  QUIC_ERR_H3_INTERNAL_ERROR,
  QUIC_ERR_H3_STREAM_CREATION_ERROR,
  QUIC_ERR_H3_CLOSED_CRITICAL_STREAM,
  QUIC_ERR_H3_FRAME_UNEXPECTED,
  QUIC_ERR_H3_FRAME_ERROR,
  QUIC_ERR_H3_EXCESSIVE_LOAD,
  QUIC_ERR_H3_ID_ERROR,
  QUIC_ERR_H3_SETTINGS_ERROR,
  QUIC_ERR_H3_MISSING_SETTINGS,
  QUIC_ERR_H3_REQUEST_REJECTED,
  QUIC_ERR_H3_REQUEST_CANCELLED,
  QUIC_ERR_H3_REQUEST_INCOMPLETE,
  QUIC_ERR_H3_MESSAGE_ERROR,
  QUIC_ERR_H3_CONNECT_ERROR,
  QUIC_ERR_H3_VERSION_FALLBACK,
  QUIC_ERR_QPACK_DECOMPRESSION_FAILED,
  QUIC_ERR_QPACK_ENCODER_STREAM_ERROR,
  QUIC_ERR_QPACK_DECODER_STREAM_ERROR,

  QUIC_PREFERRED_ADDRESS_IGNORE,
  QUIC_PREFERRED_ADDRESS_USE,

  QUIC_STREAM_PRIORITY_DEFAULT,
  QUIC_STREAM_PRIORITY_LOW,
  QUIC_STREAM_PRIORITY_HIGH,
  QUIC_STREAM_PRIORITY_FLAGS_NONE,
  QUIC_STREAM_PRIORITY_FLAGS_NON_INCREMENTAL,

  QUIC_HEADERS_KIND_INFO,
  QUIC_HEADERS_KIND_INITIAL,
  QUIC_HEADERS_KIND_TRAILING,

  QUIC_HEADERS_FLAGS_TERMINAL,
  QUIC_HEADERS_FLAGS_NONE,

  HTTP3_ALPN,

  DEFAULT_RETRYTOKEN_EXPIRATION,
  DEFAULT_TOKEN_EXPIRATION,
  DEFAULT_MAX_CONNECTIONS_PER_HOST,
  DEFAULT_MAX_CONNECTIONS,
  DEFAULT_MAX_STATELESS_RESETS,
  DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE,
  DEFAULT_UNACKNOWLEDGED_PACKET_THRESHOLD,
  DEFAULT_MAX_RETRY_LIMIT,
  DEFAULT_CIPHERS,
  DEFAULT_GROUPS,

  IDX_STATS_ENDPOINT_CREATED_AT,
  IDX_STATS_ENDPOINT_DESTROYED_AT,
  IDX_STATS_ENDPOINT_BYTES_RECEIVED,
  IDX_STATS_ENDPOINT_BYTES_SENT,
  IDX_STATS_ENDPOINT_PACKETS_RECEIVED,
  IDX_STATS_ENDPOINT_PACKETS_SENT,
  IDX_STATS_ENDPOINT_SERVER_SESSIONS,
  IDX_STATS_ENDPOINT_CLIENT_SESSIONS,
  IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT,

  IDX_STATS_SESSION_CREATED_AT,
  IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT,
  IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT,
  IDX_STATS_SESSION_GRACEFUL_CLOSING_AT,
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
  IDX_STATS_STREAM_ACKED_AT,
  IDX_STATS_STREAM_CLOSING_AT,
  IDX_STATS_STREAM_DESTROYED_AT,
  IDX_STATS_STREAM_BYTES_RECEIVED,
  IDX_STATS_STREAM_BYTES_SENT,
  IDX_STATS_STREAM_MAX_OFFSET,
  IDX_STATS_STREAM_MAX_OFFSET_ACK,
  IDX_STATS_STREAM_MAX_OFFSET_RECV,
  IDX_STATS_STREAM_FINAL_SIZE,

  IDX_STATE_ENDPOINT_LISTENING,
  IDX_STATE_ENDPOINT_CLOSING,
  IDX_STATE_ENDPOINT_WAITING_FOR_CALLBACKS,
  IDX_STATE_ENDPOINT_BUSY,
  IDX_STATE_ENDPOINT_PENDING_CALLBACKS,

  IDX_STATE_SESSION_VERSION_NEGOTIATION,
  IDX_STATE_SESSION_PATH_VALIDATION,
  IDX_STATE_SESSION_DATAGRAM,
  IDX_STATE_SESSION_SESSION_TICKET,
  IDX_STATE_SESSION_CLIENT_HELLO,
  IDX_STATE_SESSION_CLIENT_HELLO_DONE,
  IDX_STATE_SESSION_CLOSING,
  IDX_STATE_SESSION_DESTROYED,
  IDX_STATE_SESSION_GRACEFUL_CLOSING,
  IDX_STATE_SESSION_HANDSHAKE_COMPLETED,
  IDX_STATE_SESSION_HANDSHAKE_CONFIRMED,
  IDX_STATE_SESSION_OCSP,
  IDX_STATE_SESSION_OCSP_DONE,
  IDX_STATE_SESSION_SILENT_CLOSE,
  IDX_STATE_SESSION_STREAM_OPEN_ALLOWED,
  IDX_STATE_SESSION_TRANSPORT_PARAMS_SET,
  IDX_STATE_SESSION_USING_PREFERRED_ADDRESS,
  IDX_STATE_SESSION_PRIORITY_SUPPORTED,

  IDX_STATE_STREAM_ID,
  IDX_STATE_STREAM_FIN_SENT,
  IDX_STATE_STREAM_FIN_RECEIVED,
  IDX_STATE_STREAM_READ_ENDED,
  IDX_STATE_STREAM_TRAILERS,
  IDX_STATE_STREAM_DESTROYED,
  IDX_STATE_STREAM_DATA,
  IDX_STATE_STREAM_PAUSED,
  IDX_STATE_STREAM_RESET,

} = internalBinding('quic');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,

    ERR_QUIC_INTERNAL_ERROR,
    ERR_QUIC_CONNECTION_REFUSED,
    ERR_QUIC_FLOW_CONTROL_ERROR,
    ERR_QUIC_STREAM_LIMIT_ERROR,
    ERR_QUIC_STREAM_STATE_ERROR,
    ERR_QUIC_FINAL_SIZE_ERROR,
    ERR_QUIC_FRAME_ENCODING_ERROR,
    ERR_QUIC_TRANSPORT_PARAMETER_ERROR,
    ERR_QUIC_CONNECTION_ID_LIMIT_ERROR,
    ERR_QUIC_PROTOCOL_VIOLATION,
    ERR_QUIC_INVALID_TOKEN,
    ERR_QUIC_APPLICATION_ERROR,
    ERR_QUIC_CRYPTO_BUFFER_EXCEEDED,
    ERR_QUIC_KEY_UPDATE_ERROR,
    ERR_QUIC_AEAD_LIMIT_REACHED,
    ERR_QUIC_NO_VIABLE_PATH,
    ERR_QUIC_CRYPTO_ERROR,
    ERR_QUIC_VERSION_NEGOTIATION_ERROR,
    ERR_QUIC_UNKNOWN_ERROR,
    ERR_QUIC_IDLE_CLOSE,
    ERR_QUIC_UNABLE_TO_CREATE_STREAM,
    ERR_QUIC_HANDSHAKE_CANCELED,
    ERR_QUIC_ENDPOINT_FAILURE,

    ERR_QUIC_H3_GENERAL_PROTOCOL_ERROR,
    ERR_QUIC_H3_INTERNAL_ERROR,
    ERR_QUIC_H3_STREAM_CREATION_ERROR,
    ERR_QUIC_H3_CLOSED_CRITICAL_STREAM,
    ERR_QUIC_H3_FRAME_UNEXPECTED,
    ERR_QUIC_H3_FRAME_ERROR,
    ERR_QUIC_H3_EXCESSIVE_LOAD,
    ERR_QUIC_H3_ID_ERROR,
    ERR_QUIC_H3_SETTINGS_ERROR,
    ERR_QUIC_H3_MISSING_SETTINGS,
    ERR_QUIC_H3_REQUEST_REJECTED,
    ERR_QUIC_H3_REQUEST_CANCELLED,
    ERR_QUIC_H3_REQUEST_INCOMPLETE,
    ERR_QUIC_H3_MESSAGE_ERROR,
    ERR_QUIC_H3_CONNECT_ERROR,
    ERR_QUIC_H3_VERSION_FALLBACK,
    ERR_QUIC_QPACK_DECOMPRESSION_FAILED,
    ERR_QUIC_QPACK_ENCODER_STREAM_ERROR,
    ERR_QUIC_QPACK_DECODER_STREAM_ERROR,
  },
} = require('internal/errors');

const {
  kNewListener,
  kRemoveListener,
  defineEventHandler,
  EventTarget,
  Event,
} = require('internal/event_target');

const {
  kHandle: kSocketAddressHandle,
  InternalSocketAddress,
} = require('internal/socketaddress');

const {
  kHandle,
  kKeyObject,
} = require('internal/crypto/util');

const {
  InternalX509Certificate,
} = require('internal/crypto/x509');

const {
  validateBoolean,
  validateNumber,
  validateObject,
  validateString,
  validateUint32,
} = require('internal/validators');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
  kEnumerableProperty,
  createDeferredPromise,
} = require('internal/util');

const {
  isArrayBufferView,
  isCryptoKey,
  isKeyObject,
} = require('internal/util/types');

const { inspect } = require('internal/util/inspect');

const kDetach = Symbol('kDetach');
const kOwner = Symbol('kOwner');
const kCreateInstance = Symbol('kCreateEndpoint');
const kListen = Symbol('kListen');
const kFinishClose = Symbol('kFinishClose');
const kNewSession = Symbol('kNewSession');
const kClientHello = Symbol('kClientHello');
const kOcspRequest = Symbol('kOcspRequest');
const kOcspResponse = Symbol('kOcspResponse');
const kDatagram = Symbol('kDatagram');
const kHandshakeComplete = Symbol('kHandshakeComplete');
const kSessionTicket = Symbol('kSessionTicket');
const kVersionNegotiation = Symbol('kVersionNegotiation');
const kPathValidation = Symbol('kPathValidation');
const kNewStream = Symbol('kNewStream');
const kStreamReset = Symbol('kStreamReset');
const kStreamHeaders = Symbol('kStreamHeaders');
const kStreamTrailers = Symbol('kStreamTrailers');
const kStreamSendTrailers = Symbol('kStreamSendTrailers');
const kStreamBlocked = Symbol('kStreamBlocked');
const kStreamData = Symbol('kStreamData');
const kDatagramStatus = Symbol('kDatagramStatus');

/**
 * @typedef {import('../../net').SocketAddress} SocketAddress
 * @typedef {import('../crypto/keys').CryptoKey} CryptoKey
 * @typedef {import('../crypto/keys').KeyObject} KeyObject
 * @typedef {CryptoKey|KeyObject|Array<CryptoKey|Array<KeyObject>>} Key
 * @typedef {import('../crypto/x509').X509Certificate} X509Certificate
 * @typedef {import('../blob').Blob} Blob
 * @typedef {import('../webstreams/readablestream').ReadableStream} ReadableStream
 * @typedef {import('../webstreams/writablestream').WritableStream} WritableStream
 * @typedef {import('../webstreams/readablestream').QueuingStrategy} QueuingStrategy
 * @typedef {import('../../stream').Readable} Readable
 * @typedef {import('../../stream').Writable} Writable
 * @typedef {import('../fs/promises').FileHandle} FileHandle
 * @typedef {ArrayBufferViewSource, StreamSource, StreamBaseSource, BlobSource} StreamDataSource
 */

const StreamPriority = ObjectCreate(null, {
  /** @type {number} */
  DEFAULT: {
    __proto__: null,
    value: QUIC_STREAM_PRIORITY_DEFAULT,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  LOW: {
    __proto__: null,
    value: QUIC_STREAM_PRIORITY_LOW,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  HIGH: {
    __proto__: null,
    value: QUIC_STREAM_PRIORITY_HIGH,
    enumerable: true,
    configurable: false,
  }
});

const StreamPriorityFlags = ObjectCreate(null, {
  /** @type {number} */
  NONE: {
    __proto__: null,
    value: QUIC_STREAM_PRIORITY_FLAGS_NONE,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  NON_INCREMENTAL: {
    __proto__: null,
    value: QUIC_STREAM_PRIORITY_FLAGS_NON_INCREMENTAL,
    enumerable: true,
    configurable: false,
  },
});

/**
 * @readonly
 * @enum {string}
 */
const Direction = ObjectCreate(null, {
  /** @type {string} */
  BIDI: {
    __proto__: null,
    value: 'bidi',
    enumerable: true,
    configurable: false,
  },
  /** @type {string} */
  UNI: {
    __proto__: null,
    value: 'uni',
    enumerable: true,
    configurable: false,
  },
});

/**
 * @readonly
 * @enum {string}
 */
const Side = ObjectCreate(null, {
  /** @type {string} */
  CLIENT: {
    __proto__: null,
    value: 'client',
    enumerable: true,
    configurable: false,
  },
  /** @type {string} */
  SERVER: {
    __proto__: null,
    value: 'server',
    enumerable: true,
    configurable: false,
  },
});

/**
 * @readonly
 * @enum {number}
 */
const CongestionControlAlgorithm = ObjectCreate(null, {
  /** @type {number} */
  CUBIC: {
    __proto__: null,
    value: QUIC_CC_ALGO_CUBIC,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  RENO: {
    __proto__: null,
    value: QUIC_CC_ALGO_RENO,
    enumerable: true,
    configurable: false,
  },
  BBR: {
    __proto__: null,
    value: QUIC_CC_ALGO_BBR,
    enumerable: true,
    configurable: false,
  },
  BBR2: {
    __proto__: null,
    value: QUIC_CC_ALGO_BBR2,
    enumerable: true,
    configurable: false,
  },
});

/**
 * @readonly
 * @enum {number}
 */
const PreferredAddressStrategy = ObjectCreate(null, {
  /** @type {number} */
  IGNORE: {
    __proto__: null,
    value: QUIC_PREFERRED_ADDRESS_IGNORE,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  USE: {
    __proto__: null,
    value: QUIC_PREFERRED_ADDRESS_USE,
    enumerable: true,
    configurable: false,
  },
});

/**
 * @readonly
 * @enum {number}
 */
const TransportErrors = ObjectCreate(null, {
  /** @type {number} */
  NO_ERROR: {
    __proto__: null,
    value: QUIC_ERR_NO_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  INTERNAL_ERROR: {
    __proto__: null,
    value: QUIC_ERR_INTERNAL_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  CONNECTION_REFUSED: {
    __proto__: null,
    value: QUIC_ERR_CONNECTION_REFUSED,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  FLOW_CONTROL_ERROR: {
    __proto__: null,
    value: QUIC_ERR_FLOW_CONTROL_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  STREAM_LIMIT_ERROR: {
    __proto__: null,
    value: QUIC_ERR_STREAM_LIMIT_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  STREAM_STATE_ERROR: {
    __proto__: null,
    value: QUIC_ERR_STREAM_STATE_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  FINAL_SIZE_ERROR: {
    __proto__: null,
    value: QUIC_ERR_FINAL_SIZE_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  FRAME_ENCODING_ERROR: {
    __proto__: null,
    value: QUIC_ERR_FRAME_ENCODING_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  TRANSPORT_PARAMETER_ERROR: {
    __proto__: null,
    value: QUIC_ERR_TRANSPORT_PARAMETER_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  CONNECTION_ID_LIMIT_ERROR: {
    __proto__: null,
    value: QUIC_ERR_CONNECTION_ID_LIMIT_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  PROTOCOL_VIOLATION: {
    __proto__: null,
    value: QUIC_ERR_PROTOCOL_VIOLATION,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  INVALID_TOKEN: {
    __proto__: null,
    value: QUIC_ERR_INVALID_TOKEN,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  APPLICATION_ERROR: {
    __proto__: null,
    value: QUIC_ERR_APPLICATION_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  CRYPTO_BUFFER_EXCEEDED: {
    __proto__: null,
    value: QUIC_ERR_CRYPTO_BUFFER_EXCEEDED,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  KEY_UPDATE_ERROR: {
    __proto__: null,
    value: QUIC_ERR_KEY_UPDATE_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  AEAD_LIMIT_REACHED: {
    __proto__: null,
    value: QUIC_ERR_AEAD_LIMIT_REACHED,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  NO_VIABLE_PATH: {
    __proto__: null,
    value: QUIC_ERR_NO_VIABLE_PATH,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  CRYPTO_ERROR: {
    __proto__: null,
    value: QUIC_ERR_CRYPTO_ERROR,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  VERSION_NEGOTIATION_ERROR_DRAFT: {
    __proto__: null,
    value: QUIC_ERR_VERSION_NEGOTIATION_ERROR_DRAFT,
    enumerable: true,
    configurable: false,
  },
});

/**
 * @readaonly
 * @enum {number}
 */
const Http3Errors = ObjectCreate(null, {
  H3_GENERAL_PROTOCOL_ERROR: {
    __proto__: null,
    value: QUIC_ERR_H3_GENERAL_PROTOCOL_ERROR,
    enumerable: true,
    configurable: false,
  },
  H3_INTERNAL_ERROR: {
    __proto__: null,
    value: QUIC_ERR_H3_INTERNAL_ERROR,
    enumerable: true,
    configurable: false,
  },
  H3_STREAM_CREATION_ERROR: {
    __proto__: null,
    value: QUIC_ERR_H3_STREAM_CREATION_ERROR,
    enumerable: true,
    configurable: false,
  },
  H3_CLOSED_CRITICAL_STREAM: {
    __proto__: null,
    value: QUIC_ERR_H3_CLOSED_CRITICAL_STREAM,
    enumerable: true,
    configurable: false,
  },
  H3_FRAME_UNEXPECTED: {
    __proto__: null,
    value: QUIC_ERR_H3_FRAME_UNEXPECTED,
    enumerable: true,
    configurable: false,
  },
  H3_FRAME_ERROR: {
    __proto__: null,
    value: QUIC_ERR_H3_FRAME_ERROR,
    enumerable: true,
    configurable: false,
  },
  H3_EXCESSIVE_LOAD: {
    __proto__: null,
    value: QUIC_ERR_H3_EXCESSIVE_LOAD,
    enumerable: true,
    configurable: false,
  },
  H3_ID_ERROR: {
    __proto__: null,
    value: QUIC_ERR_H3_ID_ERROR,
    enumerable: true,
    configurable: false,
  },
  H3_SETTINGS_ERROR: {
    __proto__: null,
    value: QUIC_ERR_H3_SETTINGS_ERROR,
    enumerable: true,
    configurable: false,
  },
  H3_MISSING_SETTINGS: {
    __proto__: null,
    value: QUIC_ERR_H3_MISSING_SETTINGS,
    enumerable: true,
    configurable: false,
  },
  H3_REQUEST_REJECTED: {
    __proto__: null,
    value: QUIC_ERR_H3_REQUEST_REJECTED,
    enumerable: true,
    configurable: false,
  },
  H3_REQUEST_CANCELLED: {
    __proto__: null,
    value: QUIC_ERR_H3_REQUEST_CANCELLED,
    enumerable: true,
    configurable: false,
  },
  H3_REQUEST_INCOMPLETE: {
    __proto__: null,
    value: QUIC_ERR_H3_REQUEST_INCOMPLETE,
    enumerable: true,
    configurable: false,
  },
  H3_MESSAGE_ERROR: {
    __proto__: null,
    value: QUIC_ERR_H3_MESSAGE_ERROR,
    enumerable: true,
    configurable: false,
  },
  H3_CONNECT_ERROR: {
    __proto__: null,
    value: QUIC_ERR_H3_CONNECT_ERROR,
    enumerable: true,
    configurable: false,
  },
  H3_VERSION_FALLBACK: {
    __proto__: null,
    value: QUIC_ERR_H3_VERSION_FALLBACK,
    enumerable: true,
    configurable: false,
  },
  QPACK_DECOMPRESSION_FAILED: {
    __proto__: null,
    value: QUIC_ERR_QPACK_DECOMPRESSION_FAILED,
    enumerable: true,
    configurable: false,
  },
  QPACK_ENCODER_STREAM_ERROR: {
    __proto__: null,
    value: QUIC_ERR_QPACK_ENCODER_STREAM_ERROR,
    enumerable: true,
    configurable: false,
  },
  QPACK_DECODER_STREAM_ERROR: {
    __proto__: null,
    value: QUIC_ERR_QPACK_DECODER_STREAM_ERROR,
    enumerable: true,
    configurable: false,
  },
});

const constants = ObjectCreate(null, {
  /** @type {string} */
  HTTP3_ALPN: {
    __proto__: null,
    value: HTTP3_ALPN,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  MAX_CID: {
    __proto__: null,
    value: QUIC_MAX_CIDLEN,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_RETRYTOKEN_EXPIRATION: {
    __proto__: null,
    value: DEFAULT_RETRYTOKEN_EXPIRATION,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_TOKEN_EXPIRATION: {
    __proto__: null,
    value: DEFAULT_TOKEN_EXPIRATION,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_MAX_CONNECTIONS_PER_HOST: {
    __proto__: null,
    value: DEFAULT_MAX_CONNECTIONS_PER_HOST,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_MAX_CONNECTIONS: {
    __proto__: null,
    value: DEFAULT_MAX_CONNECTIONS,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_MAX_STATELESS_RESETS: {
    __proto__: null,
    value: DEFAULT_MAX_STATELESS_RESETS,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE: {
    __proto__: null,
    value: DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_MAX_RETRY_LIMIT: {
    __proto__: null,
    value: DEFAULT_MAX_RETRY_LIMIT,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_UNACKNOWLEDGED_PACKET_THRESHOLD: {
    __proto__: null,
    value: DEFAULT_UNACKNOWLEDGED_PACKET_THRESHOLD,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_CIPHERS: {
    __proto__: null,
    value: DEFAULT_CIPHERS,
    enumerable: true,
    configurable: false,
  },
  /** @type {number} */
  DEFAULT_GROUPS: {
    __proto__: null,
    value: DEFAULT_GROUPS,
    enumerable: true,
    configurable: false,
  },
  CongestionControlAlgorithm: {
    __proto__: null,
    value: CongestionControlAlgorithm,
    enumerable: true,
    configurable: false,
  },
  TransportErrors: {
    __proto__: null,
    value: TransportErrors,
    enumerable: true,
    configurable: false,
  },
  Http3Errors: {
    __proto__: null,
    value: Http3Errors,
    enumerable: true,
    configurable: false,
  },
  PreferredAddressStrategy: {
    __proto__: null,
    value: PreferredAddressStrategy,
    enumerable: true,
    configurable: false,
  },
  Direction: {
    __proto__: null,
    value: Direction,
    enumerable: true,
    configurable: false,
  },
  Side: {
    __proto__: null,
    value: Side,
    enumerable: true,
    configurable: false,
  },
  StreamPriority: {
    __proto__: null,
    value: StreamPriority,
    enumerable: true,
    configurable: false,
  },
  StreamPriorityFlags: {
    __proto__: null,
    value: StreamPriorityFlags,
    enumerable: true,
    configurable: false,
  }
});

// ======================================================================================
// Callbacks
function onEndpointDone() {
  this[kOwner][kFinishClose]();
}

function onEndpointError(context, code) {
  this[kOwner][kFinishClose](makeEndpointError(context, code));
}

function onSessionNew(session) {
  this[kOwner][kNewSession](session);
}

function onSessionClientHello(alpn, servername, ciphers) {
  this[kOwner][kClientHello](alpn, servername, ciphers);
}

function onSessionOcspRequest() {
  this[kOwner][kOcspRequest]();
}

function onSessionOcspResponse(response) {
  this[kOwner][kOcspResponse](response);
}

function onSessionDatagram(datagram, isEarly) {
  this[kOwner][kDatagram](datagram, isEarly);
}

function onSessionClose() {
  this[kOwner][kFinishClose]();
}

function onSessionError(type, code, reason) {
  this[kOwner][kFinishClose](makeQuicError(code, type, reason));
}

function onSessionHandshake(servername, alpn, cipherName, cipherVersion,
                            validationErrorReason, validationErrorCode,
                            allowEarlyData) {
  this[kOwner][kHandshakeComplete](
    servername, alpn, cipherName, cipherVersion, validationErrorReason,
    validationErrorCode, allowEarlyData);
}

function onSessionTicket(sessionTicket) {
  this[kOwner][kSessionTicket](sessionTicket);
}

function onSessionVersionNegotiation(current, requested, supported) {
  this[kOwner][kVersionNegotiation](current, requested, supported);
}

function onSessionPathValidation(result, localAddress, remoteAddress, isPreferredAddress) {
  this[kOwner][kPathValidation](
    result,
    new InternalSocketAddress(localAddress),
    new InternalSocketAddress(remoteAddress),
    isPreferredAddress);
}

function onStreamCreated(stream) {
  this[kOwner][kNewStream](stream);
}

function onStreamClose() {
  this[kOwner][kFinishClose]();
}

function onStreamError(type, code, reason) {
  this[kOwner][kFinishClose](makeQuicError(code, type, reason));
}

function onStreamReset(type, code, reason) {
  this[kOwner][kStreamReset](makeQuicError(code, type, reason));
}

function onStreamBlocked() {
  this[kOwner][kStreamBlocked]();
}

function onStreamHeaders(headers, kind) {
  this[kOwner][kStreamHeaders](headers, kind);
}

function onStreamTrailers() {
  this[kOwner][kStreamTrailers]();
}

function onStreamData(chunks, ended) {
  this[kOwner][kStreamData](chunks, ended);
}

function onSessionDatagramAcknowledged(datagram) {
  this[kOwner][kDatagramStatus](datagram, false);
}

function onSessionDatagramLost(datagram) {
  this[kOwner][kDatagramStatus](datagram, true);
}

setCallbacks({
  onEndpointDone,
  onEndpointError,
  onSessionNew,
  onSessionClientHello,
  onSessionClose,
  onSessionError,
  onSessionDatagram,
  onSessionHandshake,
  onSessionOcspRequest,
  onSessionOcspResponse,
  onSessionTicket,
  onSessionVersionNegotiation,
  onSessionPathValidation,
  onStreamBlocked,
  onStreamClose,
  onStreamCreated,
  onStreamData,
  onStreamError,
  onStreamHeaders,
  onStreamReset,
  onStreamTrailers,
  onSessionDatagramAcknowledged,
  onSessionDatagramLost,
});

// ======================================================================================

function makeQuicError(code, type, reason) {
  const reasonString = reason ? `: ${reason}` : '';
  switch (type) {
    case QUIC_ERROR_TYPE_TRANSPORT: {
      switch (code) {
        case TransportErrors.INTERNAL_ERROR:
          return new ERR_QUIC_INTERNAL_ERROR(reasonString);
        case TransportErrors.CONNECTION_REFUSED:
          return new ERR_QUIC_CONNECTION_REFUSED(reasonString);
        case TransportErrors.FLOW_CONTROL_ERROR:
          return new ERR_QUIC_FLOW_CONTROL_ERROR(reasonString);
        case TransportErrors.STREAM_LIMIT_ERROR:
          return new ERR_QUIC_STREAM_LIMIT_ERROR(reasonString);
        case TransportErrors.STREAM_STATE_ERROR:
          return new ERR_QUIC_STREAM_STATE_ERROR(reasonString);
        case TransportErrors.FINAL_SIZE_ERROR:
          return new ERR_QUIC_FINAL_SIZE_ERROR(reasonString);
        case TransportErrors.FRAME_ENCODING_ERROR:
          return new ERR_QUIC_FRAME_ENCODING_ERROR(reasonString);
        case TransportErrors.TRANSPORT_PARAMETER_ERROR:
          return new ERR_QUIC_TRANSPORT_PARAMETER_ERROR(reasonString);
        case TransportErrors.CONNECTION_ID_LIMIT_ERROR:
          return new ERR_QUIC_CONNECTION_ID_LIMIT_ERROR(reasonString);
        case TransportErrors.PROTOCOL_VIOLATION:
          return new ERR_QUIC_PROTOCOL_VIOLATION(reasonString);
        case TransportErrors.INVALID_TOKEN:
          return new ERR_QUIC_INVALID_TOKEN(reasonString);
        case TransportErrors.APPLICATION_ERROR:
          return new ERR_QUIC_APPLICATION_ERROR(reasonString);
        case TransportErrors.CRYPTO_BUFFER_EXCEEDED:
          return new ERR_QUIC_CRYPTO_BUFFER_EXCEEDED(reasonString);
        case TransportErrors.KEY_UPDATE_ERROR:
          return new ERR_QUIC_KEY_UPDATE_ERROR(reasonString);
        case TransportErrors.AEAD_LIMIT_REACHED:
          return new ERR_QUIC_AEAD_LIMIT_REACHED(reasonString);
        case TransportErrors.NO_VIABLE_PATH:
          return new ERR_QUIC_NO_VIABLE_PATH(reasonString);
        case TransportErrors.CRYPTO_ERROR:
          return new ERR_QUIC_CRYPTO_ERROR(reasonString);
      }
      break;
    }
    case QUIC_ERROR_TYPE_APPLICATION: {
      switch (code) {
        case Http3Errors.H3_GENERAL_PROTOCOL_ERROR:
          return new ERR_QUIC_H3_GENERAL_PROTOCOL_ERROR(reasonString);
        case Http3Errors.H3_INTERNAL_ERROR:
          return new ERR_QUIC_H3_INTERNAL_ERROR(reasonString);
        case Http3Errors.H3_STREAM_CREATION_ERROR:
          return new ERR_QUIC_H3_STREAM_CREATION_ERROR(reasonString);
        case Http3Errors.H3_CLOSED_CRITICAL_STREAM:
          return new ERR_QUIC_H3_CLOSED_CRITICAL_STREAM(reasonString);
        case Http3Errors.H3_FRAME_UNEXPECTED:
          return new ERR_QUIC_H3_FRAME_UNEXPECTED(reasonString);
        case Http3Errors.H3_FRAME_ERROR:
          return new ERR_QUIC_H3_FRAME_ERROR(reasonString);
        case Http3Errors.H3_EXCESSIVE_LOAD:
          return new ERR_QUIC_H3_EXCESSIVE_LOAD(reasonString);
        case Http3Errors.H3_ID_ERROR:
          return new ERR_QUIC_H3_ID_ERROR(reasonString);
        case Http3Errors.H3_SETTINGS_ERROR:
          return new ERR_QUIC_H3_SETTINGS_ERROR(reasonString);
        case Http3Errors.H3_MISSING_SETTINGS:
          return new ERR_QUIC_H3_MISSING_SETTINGS(reasonString);
        case Http3Errors.H3_REQUEST_REJECTED:
          return new ERR_QUIC_H3_REQUEST_REJECTED(reasonString);
        case Http3Errors.H3_REQUEST_CANCELLED:
          return new ERR_QUIC_H3_REQUEST_CANCELLED(reasonString);
        case Http3Errors.H3_REQUEST_INCOMPLETE:
          return new ERR_QUIC_H3_REQUEST_INCOMPLETE(reasonString);
        case Http3Errors.H3_MESSAGE_ERROR:
          return new ERR_QUIC_H3_MESSAGE_ERROR(reasonString);
        case Http3Errors.H3_CONNECT_ERROR:
          return new ERR_QUIC_H3_CONNECT_ERROR(reasonString);
        case Http3Errors.H3_VERSION_FALLBACK:
          return new ERR_QUIC_H3_VERSION_FALLBACK(reasonString);
        case Http3Errors.QPACK_DECOMPRESSION_FAILED:
          return new ERR_QUIC_QPACK_DECOMPRESSION_FAILED(reasonString);
        case Http3Errors.QPACK_ENCODER_STREAM_ERROR:
          return new ERR_QUIC_QPACK_ENCODER_STREAM_ERROR(reasonString);
        case Http3Errors.QPACK_DECODER_STREAM_ERROR:
          return new ERR_QUIC_QPACK_DECODER_STREAM_ERROR(reasonString);
      }
      break;
    }
    case QUIC_ERROR_TYPE_VERSION_NEGOTIATION:
      return new ERR_QUIC_VERSION_NEGOTIATION_ERROR(reasonString);
    case QUIC_ERROR_TYPE_IDLE_CLOSE:
      return new ERR_QUIC_IDLE_CLOSE(reasonString);
  }
  return new ERR_QUIC_UNKNOWN_ERROR(code, reasonString);
}

function makeEndpointError(context, code) {
  switch (context) {
    case QUIC_ENDPOINT_CLOSE_CONTEXT_CLOSE: return undefined;
    case QUIC_ENDPOINT_CLOSE_CONTEXT_BIND_FAILURE:
      return new ERR_QUIC_ENDPOINT_FAILURE(code, 'bind failed');
    case QUIC_ENDPOINT_CLOSE_CONTEXT_START_FAILURE:
      return new ERR_QUIC_ENDPOINT_FAILURE(code, 'start failed');
    case QUIC_ENDPOINT_CLOSE_CONTEXT_RECEIVE_FAILURE:
      return new ERR_QUIC_ENDPOINT_FAILURE(code, 'receive failed');
    case QUIC_ENDPOINT_CLOSE_CONTEXT_SEND_FAILURE:
      return new ERR_QUIC_ENDPOINT_FAILURE(code, 'send failed');
    case QUIC_ENDPOINT_CLOSE_CONTEXT_LISTEN_FAILURE:
      return new ERR_QUIC_ENDPOINT_FAILURE(code, 'listen failed');
  }
  return new ERR_QUIC_UNKNOWN_ERROR(code, 'unknown');
}

// ======================================================================================
// Validators
function validateSocketAddress(address, name) {
  if (address[kSocketAddressHandle] === undefined)
    throw new ERR_INVALID_ARG_TYPE(name, 'SocketAddress', address);
}

function validateUint8(value, name) {
  if (typeof value !== 'number' || value < 0 || value > 0xff)
    throw new ERR_INVALID_ARG_TYPE(name, 'octet', value);
}

function validateUint64(value, name) {
  if ((typeof value === 'bigint' || NumberIsInteger(value)) && BigInt(value) >= 0n) return;
  throw new ERR_INVALID_ARG_TYPE(name, 'unsigned 64-bit integer (bigint)', value);
}

function validateStreamPriority(value, name) {
  if (value === StreamPriority.DEFAULT ||
      value === StreamPriority.LOW ||
      value === StreamPriority.HIGH) return;
  throw new ERR_INVALID_ARG_TYPE(name, 'StreamPriority', value);
}

function validateStreamPriorityFlags(value, name) {
  if (value === StreamPriorityFlags.NONE ||
      value === StreamPriorityFlags.NON_INCREMENTAL) return;
  throw new ERR_INVALID_ARG_TYPE(name, 'StreamPriorityFlags', value);
}

function validateDirection(value, name) {
  if (value === Direction.BIDI || value === Direction.UNI) return;
  throw new ERR_INVALID_ARG_TYPE(name, 'Direction', value);
}

function validateCongestionControlAlgorithm(value, name) {
  if (value === CongestionControlAlgorithm.CUBIC ||
      value === CongestionControlAlgorithm.RENO) return;
  throw new ERR_INVALID_ARG_TYPE(name, 'CongestionControlAlgorithm', value);
}

function validatePreferredAddressStrategy(value, name) {
  if (value === PreferredAddressStrategy.IGNORE ||
      value === PreferredAddressStrategy.USE) return;
  throw new ERR_INVALID_ARG_TYPE(name, 'PreferredAddressStrategy', value);
}

function validateKey(key, name) {
  if (isCryptoKey(key) || isKeyObject(key)) return;
  throw new ERR_INVALID_ARG_TYPE(
    name,
    [
      'CryptoKey',
      'KeyObject',
      'Array<CryptoKey>',
      'Array<KeyObject>',
    ], key);
}

function validateArrayBufferView(value, name) {
  if (isArrayBufferView(value)) return;
  throw new ERR_INVALID_ARG_TYPE(name, 'ArrayBufferView', value);
}

function validateSessionOptions(value, name) {
  if (typeof value?.[kCreateInstance] === 'function' &&
      value?.constructor?.name === 'SessionOptions') return;
  throw new ERR_INVALID_ARG_TYPE(name, 'SessionOptions', value);
}

function validateEndpointOptions(value, name) {
  if (typeof value?.[kCreateInstance] === 'function' &&
      value?.constructor?.name === 'EndpointOptions') return;
  throw new ERR_INVALID_ARG_TYPE(name, 'EndpointOptions', value);
}

// ======================================================================================
// Events

/**
 * Indicates that an Endpoint, Session, or Stream has closed. The close
 * event will only ever be emitted once. The event may be followed by
 * the error event.
 * @event close
 */
class CloseEvent extends Event {
  constructor() {
    super('close');
  }
}

/**
 * Indicates that an error has occurred causing the Endpoint, Session, or
 * Stream to be closed. The error event will always be emitted after the
 * close event.
 * @event error
 * @property {any} error
 */
class ErrorEvent extends Event {
  #error;
  constructor(error) {
    super('error');
    this.#error = error;
  }

  get error() { return this.#error; }
}

ObjectDefineProperties(ErrorEvent.prototype, {
  error: kEnumerableProperty,
});

/**
 * Indicates that a new server-side Session has been initiated by an Endpoint.
 * @event session
 * @property {Session} session
 */
class SessionEvent extends Event {
  #session;
  #endpoint;
  constructor(endpoint, session) {
    super('session');
    this.#endpoint = endpoint;
    this.#session = session;
  }

  /** @type {Endpoint} */
  get endpoint() { return this.#endpoint; }

  /** @type {Session} */
  get session() { return this.#session; }
}

ObjectDefineProperties(SessionEvent.prototype, {
  session: kEnumerableProperty,
  endpoint: kEnumerableProperty,
});

/**
 * Indicates that a new peer-initiated stream has been opened on the session.
 * @event stream
 * @property {Stream} stream
 */
class StreamEvent extends Event {
  #stream;
  #session;
  constructor(session, stream) {
    super('stream');
    this.#stream = stream;
    this.#session = session;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {Stream} */
  get stream() { return this.#stream; }
}

ObjectDefineProperties(StreamEvent.prototype, {
  stream: kEnumerableProperty,
  session: kEnumerableProperty,
});

/**
 * Indicates that a datagram has been received on the session.
 * @event datagram
 * @property {Datagram} datagram
 */
class DatagramEvent extends Event {
  #datagram;
  #session;
  #early;
  constructor(session, datagram, early) {
    super('datagram');
    this.#session = session;
    this.#datagram = datagram;
    this.#early = early;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {Uint8Array} */
  get datagram() { return this.#datagram; }

  /** @type {boolean} */
  get early() { return this.#early; }
}

ObjectDefineProperties(DatagramEvent.prototype, {
  datagram: kEnumerableProperty,
  session: kEnumerableProperty,
  early: kEnumerableProperty,
});

/**
 * @event datagram-status
 */
class DatagramStatusEvent extends Event {
  #lost;
  #session;
  #datagram;

  constructor(session, datagram, lost) {
    super('datagram-status');
    this.#session = session;
    this.#datagram = datagram;
    this.#lost = lost;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {bigint} */
  get datagram() { return this.#datagram; }

  /** @type {boolean} */
  get lost() { return this.#lost; }
}

ObjectDefineProperties(DatagramStatusEvent.prototype, {
  datagram: kEnumerableProperty,
  session: kEnumerableProperty,
  lost: kEnumerableProperty,
});

/**
 * When OCSP is enabled for a session, this event is emitted only by server
 * Sessions when an OCSP request has been received during the TLS handshake.
 * The handshake will be paused until the done() method is called providing
 * a response to the OCSP request. If done() is not called, the handshake will
 * timeout and the session will be closed.
 * @event ocsp
 */
class OCSPRequestEvent extends Event {
  #handle;
  #session;
  #done = false;
  constructor(handle, session) {
    super('ocsp');
    this.#handle = handle;
    this.#session = session;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  done() {
    if (this.#done)
      throw new ERR_INVALID_STATE('This OCSP request is already complete.');
    this.#done = true;
    this.#handle.onOCSPDone();
  }
}

ObjectDefineProperties(OCSPRequestEvent.prototype, {
  session: kEnumerableProperty,
});

/**
 * When OCSP is enabled for a session, this event is emitted only by client
 * sessions when an OCSP response has been received during the TLS handshake.
 * @event ocsp
 * @property {Uint8Array} response
 */
class OCSPResponseEvent extends Event {
  #response;
  #session;
  constructor(session, response) {
    super('ocsp');
    this.#session = session;
    this.#response = response;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {Uint8Array} */
  get response() { return this.#response; }
}

ObjectDefineProperties(OCSPResponseEvent.prototype, {
  response: kEnumerableProperty,
  session: kEnumerableProperty,
});

/**
 * Indicates the start of a TLS handshake. The client-hello event is only
 * emitted by Server sessions and provides the ability to specify new
 * keys or certificates to use for the session based on the ALPN and
 * servername requested. The TLS handshake will be paused until the done()
 * method is called. If done() is not called, the handshake will timeout and
 * the session will be closed.
 * @event client-hello
 */
class ClientHelloEvent extends Event {
  #handle;
  #alpn;
  #servername;
  #ciphers;
  #session;
  #done = false;
  constructor(handle, session, alpn, servername, ciphers) {
    super('client-hello');
    this.#handle = handle;
    this.#session = session;
    this.#alpn = alpn;
    this.#servername = servername;
    this.#ciphers = ciphers;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {string} */
  get alpn() { return this.#alpn; }

  /** @type {string} */
  get servername() { return this.#servername; }

  /**
   * @typedef {{
   *   name: string,
   *   standardName: string,
   *   version: string,
   * }} Cipher
   * @type {Cipher[]}
   */
  get ciphers() { return this.#ciphers; }

  done() {
    if (this.#done)
      throw new ERR_INVALID_STATE('This client hello is already complete.');
    this.#done = true;
    this.#handle.onClientHelloDone();
  }
}

ObjectDefineProperties(ClientHelloEvent.prototype, {
  session: kEnumerableProperty,
  alpn: kEnumerableProperty,
  servername: kEnumerableProperty,
  ciphers: kEnumerableProperty,
});

/**
 * Emitted on client Sessions when a new SessionTicket is available.
 * SessionTicket's are used for TLS session resumption.
 * @event session-ticket
 */
class SessionTicketEvent extends Event {
  #sessionTicket;
  #session;
  constructor(session, ticket) {
    super('session-ticket');
    this.#sessionTicket = ticket;
    this.#session = session;
  }

  /**
   * @typedef {{}} SessionTicket
   * @type {SessionTicket}
   */
  get ticket() { return this.#sessionTicket; }

  /** @type {Session} */
  get session() { return this.#session; }
}

ObjectDefineProperties(SessionTicketEvent.prototype, {
  ticket: kEnumerableProperty,
  session: kEnumerableProperty,
});

/**
 * Emitted when path validation is enabled for a session and a network
 * path validation result is available.
 * @event 'path-validation'
 */
class PathValidationEvent extends Event {
  #result;
  #localAddress;
  #remoteAddress;
  #preferredAddress;
  #session;

  constructor(session, result, localAddress, remoteAddress, isPreferredAddress) {
    super('path-validation');
    this.#session = session;
    this.#result = result;
    this.#localAddress = localAddress;
    this.#remoteAddress = remoteAddress;
    this.#preferredAddress = isPreferredAddress;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {string} */
  get result() { return this.#result; }

  /** @type {SocketAddress} */
  get localAddress() { return this.#localAddress; }

  /** @type {SocketAddress} */
  get remoteAddress() { return this.#remoteAddress; }

  /** @type {boolean} */
  get preferredAddress() { return this.#preferredAddress; }

}

ObjectDefineProperties(PathValidationEvent.prototype, {
  result: kEnumerableProperty,
  localAddress: kEnumerableProperty,
  remoteAddress: kEnumerableProperty,
  preferredAddress: kEnumerableProperty,
  session: kEnumerableProperty,
});

/**
 * Emitted by client Sessions when a session request has been refused by the
 * server due to a version mismatch.
 * @event 'version-negotiation'
 */
class VersionNegotiationEvent extends Event {
  #current;
  #requested;
  #supported;
  #session;

  constructor(session, current, requested, supported) {
    super('version-negotiation');
    this.#session = session;
    this.#current = current;
    this.#requested = requested;
    this.#supported = supported;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {number} */
  get current() { return this.#current; }

  /** @type {number[]} */
  get requested() { return this.#requested; }

  /** @type {number[]} */
  get supported() { return this.#supported; }
}

ObjectDefineProperties(VersionNegotiationEvent.prototype, {
  current: kEnumerableProperty,
  requested: kEnumerableProperty,
  supported: kEnumerableProperty,
  session: kEnumerableProperty,
});

/**
 * Emitted when the TLS handshake has been completed.
 * @event 'handshake-complete'
 */
class HandshakeCompleteEvent extends Event {
  #session;
  #servername;
  #alpn;
  #cipherName;
  #cipherVersion;
  #validationErrorReason;
  #validationErrorCode;
  #allowEarlyData;

  constructor(session, servername, alpn, cipherName, cipherVersion,
              validationErrorReason, validationErrorCode,
              allowEarlyData) {
    super('handshake-complete');
    this.#session = session;
    this.#servername = servername;
    this.#alpn = alpn;
    this.#cipherName = cipherName;
    this.#cipherVersion = cipherVersion;
    this.#validationErrorReason = validationErrorReason;
    this.#validationErrorCode = validationErrorCode;
    this.#allowEarlyData = allowEarlyData;
  }

  /** @type {Session} */
  get session() { return this.#session; }

  /** @type {string} */
  get servername() { return this.#servername; }

  /** @type {string} */
  get alpn() { return this.#alpn; }

  /** @type {string} */
  get cipherName() { return this.#cipherName; }

  /** @type {string} */
  get cipherVersion() { return this.#cipherVersion; }

  /** @type {number} */
  get validationErrorReason() { return this.#validationErrorReason; }

  /** @type {number} */
  get validationErrorCode() { return this.#validationErrorCode; }

  /** @type {boolean} */
  get allowEarlyData() { return this.#allowEarlyData; }
}

ObjectDefineProperties(HandshakeCompleteEvent.prototype, {
  servername: kEnumerableProperty,
  alpn: kEnumerableProperty,
  cipherName: kEnumerableProperty,
  cipherVersion: kEnumerableProperty,
  validationErrorReason: kEnumerableProperty,
  validationErrorCode: kEnumerableProperty,
  allowEarlyData: kEnumerableProperty,
});

/**
 * Emitted on a stream when a stream reset has been received from the peer.
 * @event stream-reset
 */
class StreamResetEvent extends Event {
  #stream;
  #error;
  constructor(stream, error) {
    super('stream-reset');
    this.#stream = stream;
    this.#error = error;
  }

  /** @type {Stream} */
  get stream() { return this.#stream; }

  /** @type {any} */
  get error() { return this.#error; }
}

ObjectDefineProperties(StreamResetEvent.prototype, {
  error: kEnumerableProperty,
  stream: kEnumerableProperty,
});

/**
 * Emitted when a block of headers has been received for the stream.
 * @event headers
 */
class HeadersEvent extends Event {
  #stream;
  #headers;
  #kind;

  constructor(stream, headers, kind) {
    super('headers');
    this.#stream = stream;
    this.#headers = headers;
    this.#kind = kind;
  }

  /** @type {Stream} */
  get stream() { return this.#stream; }

  /** @type {Record<string,string>} */
  get headers() { return this.#headers; }

  /** @type {HeadersKind} */
  get kind() { return this.#kind; }
}

ObjectDefineProperties(HeadersEvent.prototype, {
  headers: kEnumerableProperty,
  kind: kEnumerableProperty,
  stream: kEnumerableProperty,
});

/**
 * Emitted when the underlying session is ready to receive trailing headers
 * for the stream. The trailers are provided by calling the send() method
 * on the TrailersEvent object. The stream will be held open until the
 * trailers are provided or the idle timeout elapses.
 * @event trailers
 */
class TrailersEvent extends Event {
  #stream;
  #done = false;
  constructor(stream) {
    super('trailers');
    this.#stream = stream;
  }

  /** @type {Stream} */
  get stream() { return this.#stream; }

  /**
   * @param {Record<string, string> trailers} trailers
   */
  send(trailers) {
    if (this.#done)
      throw new ERR_INVALID_STATE('The trailers have already been sent');
    this.#done = true;
    this.#stream[kStreamSendTrailers](trailers);
  }
}

ObjectDefineProperties(TrailersEvent.prototype, {
  stream: kEnumerableProperty,
});

/**
 * @event data
 */
class DataEvent extends Event {
  #stream;
  #chunks;
  #ended;
  constructor(stream, chunks, ended) {
    super('data');
    this.#stream = stream;
    this.#chunks = chunks;
    this.#ended = ended;
  }

  /** @type {Stream} */
  get stream() { return this.#stream; }

  /** @type {Uint8Array[]} */
  get chunks() { return this.#chunks; }

  /** @type {boolean} */
  get ended() { return this.#ended; }
}

ObjectDefineProperties(DataEvent.prototype, {
  chunks: kEnumerableProperty,
  ended: kEnumerableProperty,
  stream: kEnumerableProperty,
});


// ======================================================================================
// Stream
class StreamStats {
  #inner;
  #detached = false;

  constructor(stats) {
    this.#inner = stats;
  }

  /** @type {boolean} */
  get detached() { return this.#detached; }

  /** @type {bigint} */
  get createdAt() {
    return this.#inner[IDX_STATS_STREAM_CREATED_AT];
  }

  /** @type {bigint} */
  get acknowledgedAt() {
    return this.#inner[IDX_STATS_STREAM_ACKED_AT];
  }

  /** @type {bigint} */
  get closingAt() {
    return this.#inner[IDX_STATS_STREAM_CLOSING_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    return this.#inner[IDX_STATS_STREAM_DESTROYED_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    return this.#inner[IDX_STATS_STREAM_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bytesSent() {
    return this.#inner[IDX_STATS_STREAM_BYTES_SENT];
  }

  /** @type {bigint} */
  get maxOffset() {
    return this.#inner[IDX_STATS_STREAM_MAX_OFFSET];
  }

  /** @type {bigint} */
  get maxOffsetAcknowledged() {
    return this.#inner[IDX_STATS_STREAM_MAX_OFFSET_ACK];
  }

  /** @type {bigint} */
  get maxOffsetReceived() {
    return this.#inner[IDX_STATS_STREAM_MAX_OFFSET_RECV];
  }

  /** @type {bigint} */
  get finalSize() {
    return this.#inner[IDX_STATS_STREAM_FINAL_SIZE];
  }

  [kDetach]() {
    this.#inner = new BigUint64Array(this.#inner);
    this.#detached = true;
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `StreamStats {${inspect({
      createdAt: this.createdAt,
      acknowledgedAt: this.acknowledgedAt,
      closingAt: this.closingAt,
      destroyedAt: this.destroyedAt,
      bytesReceived: this.bytesReceived,
      bytesSent: this.bytesSent,
      maxOffset: this.maxOffset,
      maxOffsetAcknowledged: this.maxOffsetAcknowledged,
      maxOffsetReceived: this.maxOffsetReceived,
      finalSize: this.finalSize,
      detached: this.detached,
    }, opts)}}`;
  }
}

ObjectDefineProperties(StreamStats.prototype, {
  detached: kEnumerableProperty,
  createdAt: kEnumerableProperty,
  acknowledgedAt: kEnumerableProperty,
  closingAt: kEnumerableProperty,
  destroyedAt: kEnumerableProperty,
  bytesReceived: kEnumerableProperty,
  bytesSent: kEnumerableProperty,
  maxOffset: kEnumerableProperty,
  maxOffsetAcknowledged: kEnumerableProperty,
  maxOffsetReceived: kEnumerableProperty,
  finalSize: kEnumerableProperty,
});

class StreamState {
  #inner;
  constructor(state) {
    this.#inner = new DataView(state);
  }

  get id() {
    return this.#inner.getBigInt64(IDX_STATE_STREAM_ID);
  }

  get finSent() {
    return this.#inner.getUint8(IDX_STATE_STREAM_FIN_SENT);
  }

  get finReceived() {
    return this.#inner.getUint8(IDX_STATE_STREAM_FIN_RECEIVED);
  }

  get readEnded() {
    return this.#inner.getUint8(IDX_STATE_STREAM_READ_ENDED);
  }

  get trailers() {
    return this.#inner.getUint8(IDX_STATE_STREAM_TRAILERS);
  }

  get destroyed() {
    return this.#inner.getUint8(IDX_STATE_STREAM_DESTROYED);
  }

  get data() {
    return this.#inner.getUint8(IDX_STATE_STREAM_DATA);
  }

  set data(on = true) {
    this.#inner.setUint8(IDX_STATE_STREAM_DATA, on ? 1 : 0);
  }

  get paused() {
    return this.#inner.getUint8(IDX_STATE_STREAM_PAUSED);
  }

  set paused(on = true) {
    this.#inner.setUint8(IDX_STATE_STREAM_PAUSED, on ? 1 : 0);
  }

  get reset() {
    return this.#inner.getUint8(IDX_STATE_STREAM_RESET);
  }
}

class Stream extends EventTarget {
  #inner;
  #id;
  #blocked = false;
  #closed = false;
  #session;
  #stats;
  #state;
  #headers;
  #trailers;

  constructor(session, handle) {
    super();
    this.#session = session;
    this.#inner = handle;
    this.#inner[kOwner] = this;
    this.#stats = new StreamStats(this.#inner.stats);
    this.#state = new StreamState(this.#inner.state);
    this.#id = this.#state.id;
  }

  /**
   * @type {boolean}
   */
  get blocked() {
    return this.#blocked;
  }

  /**
   * @type {boolean}
   */
  get closed() {
    return this.#closed;
  }

  /**
   * @type {bigint}
   */
  get id() {
    return this.#id;
  }

  /**
   * @type {Direction}
   */
  get direction() {
    return this.#id & 0b10n ? Direction.UNI : Direction.BIDI;
  }

  /**
   * @type {Side}
   */
  get origin() {
    return this.#id & 0b01n ? Side.SERVER : Side.CLIENT;
  }

  /**
   * @type {Session}
   */
  get session() {
    return this.#session;
  }

  /**
   * @type {Record<string,string>}
   */
  get headers() {
    return this.#headers;
  }

  /**
   * @type {Record<string,string>}
   */
  get trailers() {
    return this.#trailers;
  }

  /**
   * @type {StreamStats}
   */
  get stats() {
    return this.#stats;
  }

  /**
   * @param {StreamDataSource} source The source of outbound data for this stream.
   */
  attachSource(source) {
    if (this.#inner === undefined)
      throw new ERR_INVALID_STATE('This stream is closed.');
    this.#inner.attachSource(source);
    return this;
  }

  /**
   * @returns {Stream}
   */
  resume() {
    if (this.#state !== undefined && this.#state.paused) {
      this.#state.paused = false;
      this.#inner.flushInbound();
    }
    return this;
  }

  /**
   * @returns {Stream}
   */
  pause() {
    if (this.#state !== undefined)
      this.#state.paused = true;
    return this;
  }

  /**
   * Sends a request to the peer indicating that it should stop sending data.
   * Has the side effect of closing the readable side of the stream. After this,
   * no additional data events will be received.
   * @param {bigint} [code] An application error code.
   */
  stopSending(code = 0) {
    if (this.#inner === undefined) return;
    validateUint64(code, 'code');
    this.#inner.stopSending(code);
  }

  /**
   * Sends a signal to the peer that the writable side of the stream is being
   * abruptly terminated. Has the side effect of closing the writable side of
   * stream. Data events can still occur.
   * @param {bigint} [code] An application error code.
   */
  reset(code) {
    if (this.#inner === undefined) return;
    validateUint64(code, 'code');
    this.#inner.resetStream(code);
  }

  /**
   * Sets the priority of this stream if supported by the selection alpn application.
   * @param {StreamPriority} priority
   * @param {StreamPriorityFlags} flag
   * @returns {Stream}
   */
  setPriority(priority = StreamPriority.DEFAULT, flag = StreamPriorityFlags.NONE) {
    if (!this.#session.prioritySupported || !this.#inner) return;
    validateStreamPriority(priority, 'priority');
    validateStreamPriorityFlags(flag, 'flag');
    this.#inner.setPriority(priority, flag);
    return this;
  }

  getPriority() {
    if (!this.#session.prioritySupported || !this.#inner) return StreamPriority.DEFAULT;
    return this.#inner.getPriority();
  }

  /**
   * @param {string[][]} headers
   */
  sendInfoHeaders(headers) {
    if (this.#inner === undefined)
      throw new ERR_INVALID_STATE('The stream has been closed.');
    this.#inner.sendHEaders(QUIC_HEADERS_KIND_INFO,
                            headers,
                            QUIC_HEADERS_FLAGS_NONE);
  }

  /**
   * @param {string[][]} headers
   * @param {{
   *   terminal? : boolean
   * }} options
   */
  sendInitialHeaders(headers, options = kEmptyObject) {
    if (this.#inner === undefined)
      throw new ERR_INVALID_STATE('The stream has been closed.');

    validateObject(options, 'options');
    const {
      terminal = false,
    } = options;
    validateBoolean(terminal, 'options.terminal');

    this.#inner.sendHEaders(QUIC_HEADERS_KIND_INITIAL,
                            headers,
                            terminal ? QUIC_HEADERS_FLAGS_TERMINAL : QUIC_HEADERS_FLAGS_NONE);
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `Stream {${inspect({
      id: this.id,
      direction: this.direction,
      origin: this.origin,
      closed: this.closed,
      blocked: this.blocked,
      state: this.#state,
      stats: this.#stats,
    }, opts)}}`;
  }

  [kFinishClose](maybeError) {
    this.#stats[kDetach]();
    this.#state = undefined;
    this.#inner = undefined;
    this.dispatchEvent(new CloseEvent());
    if (maybeError)
      this.dispatchEvent(new ErrorEvent(maybeError));
  }

  [kStreamReset](error) {
    // Importantly, the stream reset event is not terminal unless the
    // application wants it to be. It just means that the peer has
    // abruptly terminated their side of the stream. We can keep sending
    // unless the peer also sends a stop sending.
    this.dispatchEvent(new StreamResetEvent(this, error));
  }

  [kStreamHeaders](headers, kind) {
    this.dispatchEvent(new HeadersEvent(this, headers, kind));
  }

  [kStreamTrailers]() {
    this.dispatchEvent(new TrailersEvent(this));
  }

  [kStreamSendTrailers](trailers) {
    if (this.#inner === undefined)
      throw new ERR_INVALID_STATE('The stream has been closed.');
    // TODO(@jasnell): Convert the trailers to the appropriate structure.
    this.#inner.sendHeaders(QUIC_HEADERS_KIND_TRAILING,
                            trailers,
                            QUIC_HEADERS_FLAGS_TERMINAL);
  }

  [kStreamData](data, ended) {
    this.dispatchEvent(new DataEvent(this, data, ended));
  }

  [kNewListener](size, type, listener, once, capture, passive, weak) {
    super[kNewListener](size, type, listener, once, capture, passive, weak);

    if (this.#state === undefined) return;

    switch (type) {
      case 'data':
        this.#state.data = true;
        if (!this.#state.paused)
          this.#inner.flushInbound();
        break;
    }
  }
  [kRemoveListener](size, type, listener, capture) {
    super[kRemoveListener](size, type, listener, capture);

    if (this.#state === undefined) return;

    switch (type) {
      case 'data':
        this.#state.data = false;
        break;
    }
  }
}

ObjectDefineProperties(Stream.prototype, {
  blocked: kEnumerableProperty,
  closed: kEnumerableProperty,
  id: kEnumerableProperty,
  direction: kEnumerableProperty,
  origin: kEnumerableProperty,
  session: kEnumerableProperty,
  headers: kEnumerableProperty,
  trailers: kEnumerableProperty,
  stats: kEnumerableProperty,
});

defineEventHandler(Stream.prototype, 'close');
defineEventHandler(Stream.prototype, 'error');
defineEventHandler(Stream.prototype, 'streamreset', 'stream-reset');
defineEventHandler(Stream.prototype, 'headers');
defineEventHandler(Stream.prototype, 'trailers');
defineEventHandler(Stream.prototype, 'data');

// ======================================================================================
// Session

class SessionStats {
  #stats;
  #detached = false;

  constructor(stats) {
    this.#stats = stats;
  }

  [kDetach]() {
    this.#detached = true;
    this.stats = new BigUint64Array(this.#stats);
  }

  /** @type {boolean} */
  get detached() { return this.#detached; }

  /** @type {bigint} */
  get createdAt() {
    return this.#stats[IDX_STATS_SESSION_CREATED_AT];
  }

  /** @type {bigint} */
  get handshakeCompletedAt() {
    return this.#stats[IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT];
  }

  /** @type {bigint} */
  get handshakeConfirmedAt() {
    return this.#stats[IDX_STATS_SESSION_HANDSHAKE_CONFIRMED_AT];
  }

  /** @type {bigint} */
  get gracefulClosingAt() {
    return this.#stats[IDX_STATS_SESSION_GRACEFUL_CLOSING_AT];
  }

  /** @type {bigint} */
  get closingAt() {
    return this.#stats[IDX_STATS_SESSION_CLOSING_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    return this.#stats[IDX_STATS_SESSION_DESTROYED_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    return this.#stats[IDX_STATS_SESSION_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bytesSent() {
    return this.#stats[IDX_STATS_SESSION_BYTES_SENT];
  }

  /** @type {bigint} */
  get bidiStreamCount() {
    return this.#stats[IDX_STATS_SESSION_BIDI_STREAM_COUNT];
  }

  /** @type {bigint} */
  get uniStreamCount() {
    return this.#stats[IDX_STATS_SESSION_UNI_STREAM_COUNT];
  }

  /** @type {bigint} */
  get inboundStreamsCount() {
    return this.#stats[IDX_STATS_SESSION_STREAMS_IN_COUNT];
  }

  /** @type {bigint} */
  get outboundStreamsCount() {
    return this.#stats[IDX_STATS_SESSION_STREAMS_OUT_COUNT];
  }

  /** @type {bigint} */
  get keyUpdateCount() {
    return this.#stats[IDX_STATS_SESSION_KEYUPDATE_COUNT];
  }

  /** @type {bigint} */
  get retransmitCount() {
    return this.#stats[IDX_STATS_SESSION_LOSS_RETRANSMIT_COUNT];
  }

  /** @type {bigint} */
  get maxBytesInFlight() {
    return this.#stats[IDX_STATS_SESSION_MAX_BYTES_IN_FLIGHT];
  }

  /** @type {bigint} */
  get blockCount() {
    return this.#stats[IDX_STATS_SESSION_BLOCK_COUNT];
  }

  /** @type {bigint} */
  get bytesInFlight() {
    return this.#stats[IDX_STATS_SESSION_BYTES_IN_FLIGHT];
  }

  /** @type {bigint} */
  get congestionRecoveryStartTs() {
    return this.#stats[IDX_STATS_SESSION_CONGESTION_RECOVERY_START_TS];
  }

  /** @type {bigint} */
  get cwnd() {
    return this.#stats[IDX_STATS_SESSION_CWND];
  }

  /** @type {bigint} */
  get deliveryRate() {
    return this.#stats[IDX_STATS_SESSION_DELIVERY_RATE_SEC];
  }

  /** @type {bigint} */
  get firstRttSampleTs() {
    return this.#stats[IDX_STATS_SESSION_FIRST_RTT_SAMPLE_TS];
  }

  /** @type {bigint} */
  get initialRtt() {
    return this.#stats[IDX_STATS_SESSION_INITIAL_RTT];
  }

  /** @type {bigint} */
  get lastTxPacketTs() {
    return this.#stats[IDX_STATS_SESSION_LAST_TX_PKT_TS];
  }

  /** @type {bigint} */
  get latestRtt() {
    return this.#stats[IDX_STATS_SESSION_LATEST_RTT];
  }

  /** @type {bigint} */
  get lossDetectionTimer() {
    return this.#stats[IDX_STATS_SESSION_LOSS_DETECTION_TIMER];
  }

  /** @type {bigint} */
  get lossTime() {
    return this.#stats[IDX_STATS_SESSION_LOSS_TIME];
  }

  /** @type {bigint} */
  get maxUdpPayloadSize() {
    return this.#stats[IDX_STATS_SESSION_MAX_UDP_PAYLOAD_SIZE];
  }

  /** @type {bigint} */
  get minRtt() {
    return this.#stats[IDX_STATS_SESSION_MIN_RTT];
  }

  /** @type {bigint} */
  get ptoCount() {
    return this.#stats[IDX_STATS_SESSION_PTO_COUNT];
  }

  /** @type {bigint} */
  get rttVar() {
    return this.#stats[IDX_STATS_SESSION_RTTVAR];
  }

  /** @type {bigint} */
  get smoothedRtt() {
    return this.#stats[IDX_STATS_SESSION_SMOOTHED_RTT];
  }

  /** @type {bigint} */
  get ssThreshold() {
    return this.#stats[IDX_STATS_SESSION_SSTHRESH];
  }

  /** @type {bigint} */
  get receiveRate() {
    return this.#stats[IDX_STATS_SESSION_RECEIVE_RATE];
  }

  /** @type {bigint} */
  get sendRate() {
    return this.#stats[IDX_STATS_SESSION_SEND_RATE];
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `SessionStats {${inspect({
      createdAt: this.createdAt,
      handshakeCompletedAt: this.handshakeCompletedAt,
      handshakeConfirmedAt: this.handshakeConfirmedAt,
      lastSentAt: this.lastSentAt,
      lastReceivedAt: this.lastReceivedAt,
      gracefulClosingAt: this.gracefulClosingAt,
      closingAt: this.closingAt,
      destroyedAt: this.destroyedAt,
      bytesReceived: this.bytesReceived,
      bytesSent: this.bytesSent,
      bidiStreamCount: this.bidiStreamCount,
      uniStreamCount: this.uniStreamCount,
      inboundStreamsCount: this.inboundStreamsCount,
      outboundStreamsCount: this.outboundStreamsCount,
      keyUpdateCount: this.keyUpdateCount,
      retransmitCount: this.retransmitCount,
      maxBytesInFlight: this.maxBytesInFlight,
      blockCount: this.blockCount,
      bytesInFlight: this.bytesInFlight,
      congestionRecoveryStartTs: this.congestionRecoveryStartTs,
      cwnd: this.cwnd,
      deliveryRate: this.deliveryRate,
      firstRttSampleTs: this.firstRttSampleTs,
      initialRtt: this.initialRtt,
      lastTxPacketTs: this.lastTxPacketTs,
      latestRtt: this.latestRtt,
      lossDetectionTimer: this.lossDetectionTimer,
      lossTime: this.lossTime,
      maxUdpPayloadSize: this.maxUdpPayloadSize,
      minRtt: this.minRtt,
      ptoCount: this.ptoCount,
      rttVar: this.rttVar,
      smoothedRtt: this.smoothedRtt,
      ssThreshold: this.ssThreshold,
      receiveRate: this.receiveRate,
      sendRate: this.sendRate,
      detached: this.detached,
    }, opts)}}`;
  }
}

ObjectDefineProperties(SessionStats.prototype, {
  detached: kEnumerableProperty,
  createdAt: kEnumerableProperty,
  handshakeCompletedAt: kEnumerableProperty,
  handshakeConfirmedAt: kEnumerableProperty,
  gracefulClosingAt: kEnumerableProperty,
  closingAt: kEnumerableProperty,
  destroyedAt: kEnumerableProperty,
  bytesReceived: kEnumerableProperty,
  bytesSent: kEnumerableProperty,
  bidiStreamCount: kEnumerableProperty,
  uniStreamCount: kEnumerableProperty,
  inboundStreamsCount: kEnumerableProperty,
  outboundStreamsCount: kEnumerableProperty,
  keyUpdateCount: kEnumerableProperty,
  retransmitCount: kEnumerableProperty,
  maxBytesInFlight: kEnumerableProperty,
  blockCount: kEnumerableProperty,
  bytesInFlight: kEnumerableProperty,
  congestionRecoveryStartTs: kEnumerableProperty,
  cwnd: kEnumerableProperty,
  deliveryRate: kEnumerableProperty,
  firstRttSampleTs: kEnumerableProperty,
  initialRtt: kEnumerableProperty,
  lastTxPacketTs: kEnumerableProperty,
  latestRtt: kEnumerableProperty,
  lossDetectionTimer: kEnumerableProperty,
  lossTime: kEnumerableProperty,
  maxUdpPayloadSize: kEnumerableProperty,
  minRtt: kEnumerableProperty,
  ptoCount: kEnumerableProperty,
  rttVar: kEnumerableProperty,
  smoothedRtt: kEnumerableProperty,
  ssThreshold: kEnumerableProperty,
  receiveRate: kEnumerableProperty,
  sendRate: kEnumerableProperty,
});

class SessionState {
  #inner;

  constructor(state) {
    this.#inner = new DataView(state);
  }

  get prioritySupported() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_PRIORITY_SUPPORTED));
  }

  get versionNegotiation() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_VERSION_NEGOTIATION));
  }

  set versionNegotiation(on = true) {
    this.#inner.setUint8(IDX_STATE_SESSION_VERSION_NEGOTIATION, on ? 1 : 0);
  }

  get pathValidation() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_PATH_VALIDATION));
  }

  set pathValidation(on = true) {
    this.#inner.setUint8(IDX_STATE_SESSION_PATH_VALIDATION, on ? 1 : 0);
  }

  get datagram() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_DATAGRAM));
  }

  set datagram(on = true) {
    this.#inner.setUint8(IDX_STATE_SESSION_DATAGRAM, on ? 1 : 0);
  }

  get sessionTicket() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_SESSION_TICKET));
  }

  set sessionTicket(on) {
    this.#inner.setUint8(IDX_STATE_SESSION_SESSION_TICKET, on ? 1 : 0);
  }

  get clientHello() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_CLIENT_HELLO));
  }

  set clientHello(on) {
    this.#inner.setUint8(IDX_STATE_SESSION_CLIENT_HELLO, on ? 1 : 0);
  }

  get clientHelloDone() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_CLIENT_HELLO_DONE));
  }

  get closing() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_CLOSING));
  }

  get destroyed() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_DESTROYED));
  }

  get gracefulClosing() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_GRACEFUL_CLOSING));
  }

  get handshakeCompleted() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_HANDSHAKE_COMPLETED));
  }

  get handshakeConfirmed() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_HANDSHAKE_CONFIRMED));
  }

  get ocsp() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_OCSP));
  }

  set ocsp(on) {
    this.#inner.setUint8(IDX_STATE_SESSION_OCSP, on ? 1 : 0);
  }

  get oscpDone() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_OCSP_DONE));
  }

  get silentClose() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_SILENT_CLOSE));
  }

  get streamOpenAllowed() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_STREAM_OPEN_ALLOWED));
  }

  get transportParamsSet() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_TRANSPORT_PARAMS_SET));
  }

  get usingPreferredAddress() {
    return Boolean(this.#inner.getUint8(IDX_STATE_SESSION_USING_PREFERRED_ADDRESS));
  }
}

class SessionOptions {
  #inner;

  /**
   * @typedef {{
   *   rejectUnauthorized: boolean?,
   *   clientHello: boolean?,
   *   enableTLSTrace: boolean?,
   *   requestPeerCertificate: boolean?,
   *   ocsp: boolean?,
   *   verifyHostnameIdentity: boolean?,
   *   keylog: boolean?,
   *   sessionID: string?,
   *   ciphers: string?,
   *   groups: string?,
   *   key: Key|Array<Key>,
   *   cert: ArrayBufferView|Array<ArrayBufferView>?,
   *   ca: ArrayBufferView|Array<ArrayBufferView>?,
   *   crl: ArrayBufferView|Array<ArrayBufferView>?,
   * }} SecureOptions
   * @typedef {{
   *   maxHeaderPairs?: bigint,
   *   maxHeaderLength?: bigint,
   *   maxFieldSectionSize?: bigint,
   *   qpackBlockedStreams?: bigint,
   *   qpackMaxTableCapacity?: bigint,
   *   qpackEncoderMaxTableCapacity?: bigint,
   * }} ApplicationOptions
   * @typedef {{
   *   initialMaxStreamDataBidiLocal: bigint?,
   *   initialMaxStreamDataBidiRemote: bigint?,
   *   initialMaxStreamDataUni: bigint?,
   *   initialMaxData: bigint?,
   *   initialMaxStreamsBidi: bigint?,
   *   initialMaxStreamsUni: bigint?,
   *   maxIdleTimeout: bigint?,
   *   activeConnectionIdLimit: bigint?,
   *   ackDelayExponent: bigint?,
   *   maxAckDelay: bigint?,
   *   maxDatagramFrameSize: bigint?,
   *   disableActiveMigration: boolean?,
   * }} TransportParams
   * @typedef {{
   *   ipv4: SocketAddress?,
   *   ipv6: SocketAddress?,
   * }} PreferredAddress
   * @param {Side} side
   * @param {{
   *   alpn: string,
   *   servername: string?,
   *   preferredAddressStrategy: PreferredAddressStrategy?,
   *   qlog: boolean?
   *   secure: SecureOptions,
   *   application: ApplicationOptions?,
   *   transport: TransportParams?,
   *   preferredAddress: PreferredAddress?,
   * }} options
   */
  constructor(side, options) {
    validateObject(options, 'options', options);
    const {
      alpn,  // No default.
      servername,
      preferredAddressStrategy = PreferredAddressStrategy.USE,
      // cidFactory = undefined,  // Currently not used
      qlog = false,
      secure = kEmptyObject,
      application = kEmptyObject,
      transportParams = kEmptyObject,
      preferredAddress = kEmptyObject,
    } = options;
    validateString(alpn, 'options.alpn');
    if (side === Side.SERVER) {
      if (servername !== undefined) validateString(servername, 'options.servername');
      validatePreferredAddressStrategy(preferredAddressStrategy,
                                       'options.preferredAddressStrategy');
      // TODO: Validate cidFactory when we support that
    }
    validateBoolean(qlog, 'options.qlog');
    validateObject(secure, 'options.secure');
    validateObject(application, 'options.application');
    validateObject(transportParams, 'options.transportParams');
    if (side === Side.CLIENT) {
      validateObject(preferredAddress, 'options.preferredAddress');
    }

    const {
      rejectUnauthorized = false,
      clientHello = false,
      enableTLSTrace = false,
      requestPeerCertificate = false,
      ocsp = false,
      verifyHostnameIdentity = true,
      keylog = false,
      sessionID,
      ciphers,
      groups,
      key,
      certs,
      ca,
      crl,
    } = secure;

    const {
      maxHeaderPairs,
      maxHeaderLength,
      maxFieldSectionSize,
      qpackBlockedStreams,
      qpackMaxTableCapacity,
      qpackEncoderMaxTableCapacity,
    } = application;

    if (maxHeaderPairs !== undefined)
      validateUint64(maxHeaderPairs, 'options.application.maxHeaderPairs');
    if (maxHeaderLength !== undefined)
      validateUint64(maxHeaderLength, 'options.application.maxHeaderLength');
    if (maxFieldSectionSize !== undefined)
      validateUint64(maxFieldSectionSize, 'options.application.maxFieldSectionSize');
    if (qpackBlockedStreams !== undefined)
      validateUint64(qpackBlockedStreams, 'options.application.qpackBlockedStreams');
    if (qpackMaxTableCapacity !== undefined)
      validateUint64(qpackMaxTableCapacity, 'options.application.qpackMaxTableCapacity');
    if (qpackEncoderMaxTableCapacity !== undefined) {
      validateUint64(qpackEncoderMaxTableCapacity,
                     'options.application.qpackEncoderMaxTableCapacity');
    }

    const {
      initialMaxStreamDataBidiLocal,
      initialMaxStreamDataBidiRemote,
      initialMaxStreamDataUni,
      initialMaxData,
      initialMaxStreamsBidi,
      initialMaxStreamsUni,
      maxIdleTimeout,
      activeConnectionIdLimit,
      ackDelayExponent,
      maxAckDelay,
      maxDatagramFrameSize,
      disableActiveMigration,
    } = transportParams;

    const {
      ipv4: ipv4PreferredAddress,
      ipv6: ipv6PreferredAddress,
    } = preferredAddress;

    if (side === Side.SERVER) {
      validateBoolean(clientHello, 'options.secure.clientHello');
      if (sessionID !== undefined) validateString(sessionID, 'options.secure.sessionID');
      validateBoolean(requestPeerCertificate, 'options.secure.requestPeerCertificate');
    } else if (side === Side.CLIENT) {
      validateBoolean(rejectUnauthorized, 'options.secure.rejectUnauthorized');
      validateBoolean(verifyHostnameIdentity, 'options.secure.verifyHostnameIdentity');
    }
    validateBoolean(enableTLSTrace, 'options.secure.enableTLSTrace');
    validateBoolean(ocsp, 'options.secure.ocsp');
    validateBoolean(keylog, 'options.secure.keylog');
    if (ciphers !== undefined) validateString(ciphers, 'options.secure.ciphers');
    if (groups !== undefined) validateString(groups, 'options.secure.groups');

    const keys = [];
    if (key !== undefined) {
      if (ArrayIsArray(key)) {
        for (let i = 0; i < key.length; i++) {
          if (side === Side.SERVER || key[i] !== undefined)
            validateKey(key[i], 'options.secure.key[' + i + ']');
          if (isCryptoKey(key[i]))
            keys.push(key[i][kKeyObject][kHandle]);
          else
            keys.push(key[i][kHandle]);
        }
      } else {
        if (side === Side.SERVER || key !== undefined)
          validateKey(key, 'options.secure.key');
        if (isCryptoKey(key)) {
          keys.push(key[kKeyObject][kHandle]);
        } else if (isKeyObject(key)) {
          keys.push(key[kHandle]);
        }
      }
    } else if (side === Side.SERVER) {
      validateKey(key, 'options.secure.key');
    }

    let certsArray = [];
    if (certs !== undefined) {
      if (ArrayIsArray(certs)) {
        for (let i = 0; i < certs.length; i++)
          validateArrayBufferView(certs[i], 'options.secure.certs[' + i + ']');
        certsArray = certs;
      } else {
        validateArrayBufferView(certs, 'options.secure.certs');
        certsArray.push(certs);
      }
    }

    let caArray = [];
    if (ca !== undefined) {
      if (ArrayIsArray(ca)) {
        for (let i = 0; i < ca.length; i++)
          validateArrayBufferView(ca[i], 'options.secure.ca[' + i + ']');
        caArray = ca;
      } else {
        validateArrayBufferView(ca, 'options.secure.ca');
        caArray.push(ca);
      }
    }

    let crlArray = [];
    if (crl !== undefined) {
      if (ArrayIsArray(crl)) {
        for (let i = 0; i < crl.length; i++)
          validateArrayBufferView(crl[i], 'options.secure.crl[' + i + ']');
        crlArray = crl;
      } else {
        validateArrayBufferView(crl, 'options.secure.crl');
        crlArray.push(crl);
      }
    }

    if (initialMaxStreamDataBidiLocal !== undefined) {
      validateUint64(initialMaxStreamDataBidiLocal,
                     'options.transportParams.initialMaxStreamDataBidiLocal');
    }
    if (initialMaxStreamDataBidiRemote !== undefined) {
      validateUint64(initialMaxStreamDataBidiRemote,
                     'options.transportParams.initialMaxStreamDataBidiRemote');
    }
    if (initialMaxStreamDataUni !== undefined) {
      validateUint64(initialMaxStreamDataUni,
                     'options.transportParams.initialMaxStreamDataUni');
    }
    if (initialMaxData !== undefined) {
      validateUint64(initialMaxData,
                     'options.transportParams.initialMaxData');
    }
    if (initialMaxStreamsBidi !== undefined) {
      validateUint64(initialMaxStreamsBidi,
                     'options.transportParams.initialMaxStreamsBidi');
    }
    if (initialMaxStreamsUni !== undefined) {
      validateUint64(initialMaxStreamsUni,
                     'options.transportParams.initialMaxStreamsUni');
    }
    if (maxIdleTimeout !== undefined) {
      validateUint64(maxIdleTimeout,
                     'options.transportParams.maxIdleTimeout');
    }
    if (activeConnectionIdLimit !== undefined) {
      validateUint64(activeConnectionIdLimit,
                     'options.transportParams.activeConnectionIdLimit');
    }
    if (ackDelayExponent !== undefined) {
      validateUint64(ackDelayExponent,
                     'options.transportParams.ackDelayExponent');
    }
    if (maxAckDelay !== undefined) {
      validateUint64(maxAckDelay,
                     'options.transportParams.maxAckDelay');
    }
    if (maxDatagramFrameSize !== undefined) {
      validateUint64(maxDatagramFrameSize,
                     'options.transportParams.maxDatagramFrameSize');
    }
    if (disableActiveMigration !== undefined) {
      validateBoolean(disableActiveMigration,
                      'options.transportParams.disableActiveMigration');
    }

    if (side === Side.SERVER) {
      if (ipv4PreferredAddress !== undefined)
        validateSocketAddress(ipv4PreferredAddress, 'options.preferredAddress.ipv4');
      if (ipv6PreferredAddress !== undefined)
        validateSocketAddress(ipv6PreferredAddress, 'options.preferredAddress.ipv6');
    }

    this.#inner = new SessionOptions_(
      alpn,
      servername,
      preferredAddressStrategy,
      undefined, // Connection ID Factory, not currently used.
      qlog,
      {
        // TLS Options
        rejectUnauthorized,
        clientHello,
        enableTLSTrace,
        requestPeerCertificate,
        ocsp,
        verifyHostnameIdentity,
        keylog,
        sessionID,
        ciphers,
        groups,
        keys,
        certs: certsArray,
        ca: caArray,
        crl: crlArray,
      },
      {
        // Application Options
        maxHeaderPairs,
        maxHeaderLength,
        maxFieldSectionSize,
        qpackBlockedStreams,
        qpackMaxTableCapacity,
        qpackEncoderMaxTableCapacity,
      },
      {
        // Transport Parameters
        initialMaxStreamDataBidiLocal,
        initialMaxStreamDataBidiRemote,
        initialMaxStreamDataUni,
        initialMaxData,
        initialMaxStreamsBidi,
        initialMaxStreamsUni,
        maxIdleTimeout,
        activeConnectionIdLimit,
        ackDelayExponent,
        maxAckDelay,
        maxDatagramFrameSize,
        disableActiveMigration,
      },
      ipv4PreferredAddress?.[kSocketAddressHandle],
      ipv6PreferredAddress?.[kSocketAddressHandle]);
  }

  [kCreateInstance](endpointHandle, endpoint, address) {
    const handle = endpointHandle.connect(address[kSocketAddressHandle], this.#inner);
    if (handle === undefined)
      throw new ERR_QUIC_UNABLE_TO_CREATE_STREAM();
    return new Session(endpoint, handle);
  }

  [kListen](endpointHandle) {
    endpointHandle.listen(this.#inner);
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;
    return 'SessionOptions {}';
  }
}

class Session extends EventTarget {
  #inner;
  #endpoint;
  #stats;
  #state;
  #closePromise;
  #peerCertificate;
  #certificate;
  #address;
  #handshake = createDeferredPromise();

  constructor(endpoint, sessionHandle) {
    super();
    this.#inner = sessionHandle;
    this.#endpoint = endpoint;
    this.#inner[kOwner] = this;
    this.#stats = new SessionStats(this.#inner.stats);
    this.#state = new SessionState(this.#inner.state);
  }

  /**
   * @type {Endpoint}
   */
  get endpoint() { return this.#endpoint; }

  /**
   * @type {SocketAddress}
   */
  get address() {
    if (this.#address === undefined) {
      const ret = this.#inner?.getRemoteAddress();
      this.#address = ret === undefined ? undefined : new InternalSocketAddress(ret);
    }
    return this.#address;
  }

  /**
   * @type {X509Certificate}
   */
  get certificate() {
    if (this.#certificate === undefined) {
      const ret = this.#inner?.getCertificate();
      this.#certificate =
        ret === undefined ? undefined : new InternalX509Certificate(ret);
    }
    return this.#certificate;
  }

  /**
   * @type {X509Certificate}
   */
  get peerCertificate() {
    if (this.#peerCertificate === undefined) {
      const ret = this.#inner?.getPeerCertificate();
      this.#peerCertificate =
        ret === undefined ? undefined : new InternalX509Certificate(ret);
    }
    return this.#peerCertificate;
  }

  /**
   * @type {{}}
   */
  get ephemeralKey() { return this.#inner?.getEphemeralKeyInfo(); }

  /**
   * @type {SessionStats}
   */
  get stats() { return this.#stats; }

  /** @type {Promise<void>} */
  get handshakeCompleted() {
    return this.#handshake.promise;
  }

  get handshakeConfirmed() {
    return this.#state?.handshakeConfirmed;
  }

  /** @type {boolean} */
  get prioritySupported() {
    return this.#state?.prioritySupported;
  }

  /**
   * Initiates a graceful close of the session.
   * @async
   * @returns {Promise<void>}
   */
  close() {
    if (this.#closePromise !== undefined)
      return this.#closePromise.promise;
    this.#closePromise = createDeferredPromise();
    if (this.#inner === undefined) {
      this.#closePromise.reject(new ERR_INVALID_STATE('The session is closed.'));
    } else {
      this.#inner.gracefulClose();
    }
    return this.#closePromise.promise;
  }

  /**
   * Immediately destroy the session, causing all open streams to be abruptly terminated.
   * @param {any} [error] An optional error. If specified, the 'error' event will be triggered.
   */
  destroy(error) {
    this[kFinishClose](error);
  }

  /**
   * Initiate a key update for this session.
   */
  updateKey() {
    if (this.#inner === undefined)
      throw new ERR_INVALID_STATE('The session is closed.');
    if (this.#state?.gracefulClosing)
      throw new ERR_INVALID_STATE('The session is closing.');
    this.#inner.updateKey();
  }

  /**
   * Send a datagram if it is supported by the peer.
   * @param {ArrayBufferView} datagram
   * @returns {bigint} A bigint identifying the datagram.
   */
  send(datagram) {
    if (this.#inner === undefined)
      throw new ERR_INVALID_STATE('The session is closed.');
    if (this.#state?.gracefulClosing)
      throw new ERR_INVALID_STATE('The session is closing.');
    validateArrayBufferView(datagram, 'datagram');
    return this.#inner.sendDatagram(datagram);
  }

  /**
   * Open a new stream.
   * @param {Direction} direction = Direction.BIDIRECTIONAL
   * @returns {Stream} The new Stream.
   */
  open(direction = Direction.BIDI) {
    if (this.#inner === undefined)
      throw new ERR_INVALID_STATE('The session is closed.');
    if (this.#state?.gracefulClosing)
      throw new ERR_INVALID_STATE('The session is closing.');
    validateDirection(direction, 'direction');

    const stream = this.#inner.openStream(direction === Direction.BIDI ? 0 : 1);
    if (stream === undefined)
      throw new ERR_QUIC_UNABLE_TO_CREATE_STREAM();
    return new Stream(this, stream);
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `Session {${inspect({
      address: this.address,
      certificate: this.certificate,
      peerCertificate: this.peerCertificate,
      ephemeralKey: this.ephemeralKey,
      state: this.#state,
      stats: this.#stats,
    }, opts)}}`;
  }

  [kClientHello](...args) {
    this.dispatchEvent(new ClientHelloEvent(this.#inner, this, ...args));
  }

  [kOcspRequest]() {
    this.dispatchEvent(new OCSPRequestEvent(this.#inner, this));
  }

  [kOcspResponse](response) {
    this.dispatchEvent(new OCSPResponseEvent(this.#inner, this, response));
  }

  [kDatagram](...args) {
    if (this.#inner === undefined) return;
    this.dispatchEvent(new DatagramEvent(this, ...args));
  }

  [kFinishClose](maybeError) {
    if (!this.#state.handshakeCompleted) {
      if (maybeError) {
        this.#handshake.reject(maybeError);
      } else {
        this.#handshake.reject(new ERR_QUIC_HANDSHAKE_CANCELED());
      }
    }

    this.#stats[kDetach]();
    this.#state = undefined;
    this.#peerCertificate = undefined;
    this.#certificate = undefined;
    this.#address = undefined;

    // Finish cleaning up the Session by calling destroy...
    this.#inner.destroy();
    this.#inner = undefined;

    if (this.#closePromise !== undefined) {
      if (maybeError) {
        this.#closePromise.reject(maybeError);
      } else {
        this.#closePromise.resolve();
      }
    }

    this.dispatchEvent(new CloseEvent());

    if (maybeError)
      this.dispatchEvent(new ErrorEvent(maybeError));
  }

  [kNewStream](stream) {
    this.dispatchEvent(new StreamEvent(this, new Stream(this, stream)));
  }

  [kPathValidation](...args) {
    this.dispatchEvent(new PathValidationEvent(this, ...args));
  }

  [kSessionTicket](...args) {
    this.dispatchEvent(new SessionTicketEvent(this, ...args));
  }

  [kVersionNegotiation](...args) {
    this.dispatchEvent(new VersionNegotiationEvent(this, ...args));
  }

  [kHandshakeComplete](...args) {
    this.#handshake.resolve();
    this.dispatchEvent(new HandshakeCompleteEvent(this, ...args));
  }

  [kDatagramStatus](...args) {
    this.dispatchEvent(new DatagramStatusEvent(this, ...args));
  }

  [kNewListener](size, type, listener, once, capture, passive, weak) {
    super[kNewListener](size, type, listener, once, capture, passive, weak);

    if (this.#state === undefined) return;

    switch (type) {
      case 'datagram':
        this.#state.datagram = true;
        break;
      case 'client-hello':
        this.#state.clientHello = true;
        break;
      case 'ocsp':
        this.#state.ocsp = true;
        break;
      case 'session-ticket':
        this.#state.sessionTicket = true;
        break;
      case 'path-validation':
        this.#state.pathValidation = true;
        break;
      case 'version-negotiation':
        this.#state.versionNegotiation = true;
        break;
    }
  }
  [kRemoveListener](size, type, listener, capture) {
    super[kRemoveListener](size, type, listener, capture);

    if (this.#state === undefined) return;

    switch (type) {
      case 'datagram':
        this.#state.datagram = false;
        break;
      case 'client-hello':
        this.#state.clientHello = false;
        break;
      case 'ocsp':
        this.#state.ocsp = false;
        break;
      case 'session-ticket':
        this.#state.sessionTicket = false;
        break;
      case 'version-negotiation':
        this.#state.versionNegotiation = false;
        break;
      case 'path-validation':
        this.#state.pathValidation = false;
        break;
    }
  }
}

ObjectDefineProperties(Session.prototype, {
  endpoint: kEnumerableProperty,
  address: kEnumerableProperty,
  certificate: kEnumerableProperty,
  peerCertificate: kEnumerableProperty,
  ephemeralKey: kEnumerableProperty,
  stats: kEnumerableProperty,
  handshakeCompleted: kEnumerableProperty,
  handshakeConfirmed: kEnumerableProperty,
});

defineEventHandler(Session.prototype, 'close');
defineEventHandler(Session.prototype, 'error');
defineEventHandler(Session.prototype, 'stream');
defineEventHandler(Session.prototype, 'datagram');
defineEventHandler(Session.prototype, 'ocsp');
defineEventHandler(Session.prototype, 'pathvalidation', 'path-validation');
defineEventHandler(Session.prototype, 'handshakecomplete', 'handshake-complete');
defineEventHandler(Session.prototype, 'sessionticket', 'session-ticket');
defineEventHandler(Session.prototype, 'clienthello', 'client-hello');

// ======================================================================================
// Endpoint
class EndpointStats {
  #buffer;
  #detached = false;

  constructor(buffer) {
    this.#buffer = buffer;
  }

  /** @type {boolean} */
  get detached() { return this.#detached; }

  /* @type {bigint} */
  get createdAt() { return this.#buffer[IDX_STATS_ENDPOINT_CREATED_AT]; }
  /* @type {bigint} */
  get destroyedAt() { return this.#buffer[IDX_STATS_ENDPOINT_DESTROYED_AT]; }
  /* @type {bigint} */
  get bytesReceived() { return this.#buffer[IDX_STATS_ENDPOINT_BYTES_RECEIVED]; }
  /* @type {bigint} */
  get bytesSent() { return this.#buffer[IDX_STATS_ENDPOINT_BYTES_SENT]; }
  /* @type {bigint} */
  get packetsReceived() { return this.#buffer[IDX_STATS_ENDPOINT_PACKETS_RECEIVED]; }
  /* @type {bigint} */
  get packetsSent() { return this.#buffer[IDX_STATS_ENDPOINT_PACKETS_SENT]; }
  /* @type {bigint} */
  get serverSessions() { return this.#buffer[IDX_STATS_ENDPOINT_SERVER_SESSIONS]; }
  /* @type {bigint} */
  get clientSessions() { return this.#buffer[IDX_STATS_ENDPOINT_CLIENT_SESSIONS]; }
  /* @type {bigint} */
  get busyCount() { return this.#buffer[IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT]; }

  [kDetach]() {
    // Copy the buffer so that it is no longer tied to the original memory buffer.
    this.#buffer = new BigUint64Array(this.#buffer);
    this.#detached = true;
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `EndpointStats {${inspect({
      createdAt: this.createdAt,
      destroyedAt: this.destroyedAt,
      bytesReceived: this.bytesReceived,
      bytesSent: this.bytesSent,
      packetsReceived: this.packetsReceived,
      packetsSent: this.packetsSent,
      serverSessions: this.serverSessions,
      clientSessions: this.clientSessions,
      serverBusyCount: this.serverBusyCount,
      detached: this.detached,
    }, opts)}}`;
  }
}

ObjectDefineProperties(EndpointStats.prototype, {
  detached: kEnumerableProperty,
  createdAt: kEnumerableProperty,
  destroyedAt: kEnumerableProperty,
  bytesReceived: kEnumerableProperty,
  bytesSent: kEnumerableProperty,
  packetsReceived: kEnumerableProperty,
  packetsSent: kEnumerableProperty,
  serverSessions: kEnumerableProperty,
  clientSessions: kEnumerableProperty,
  busyCount: kEnumerableProperty,
});

class EndpointState {
  #inner;

  constructor(state) {
    this.#inner = new DataView(state);
  }

  /* @type {boolean} */
  get listening() {
    return this.#inner.getUint8(IDX_STATE_ENDPOINT_LISTENING);
  }

  /* @type {boolean} */
  get closing() {
    return this.#inner.getUint8(IDX_STATE_ENDPOINT_CLOSING);
  }

  /* @type {boolean} */
  get waitingForCallbacks() {
    return this.#inner.getUint8(IDX_STATE_ENDPOINT_WAITING_FOR_CALLBACKS);
  }

  /* @type {boolean} */
  get busy() {
    return this.#inner.getUint8(IDX_STATE_ENDPOINT_BUSY);
  }

  /* @type {bigint} */
  get pendingCallbacks() {
    return this.#inner.getBigUint64(IDX_STATE_ENDPOINT_PENDING_CALLBACKS);
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `EndpointState {${inspect({
      listening: this.listening,
      closing: this.closing,
      waitingForCallbacks: this.waitingForCallbacks,
      busy: this.busy,
      pendingCallbacks: this.pendingCallbacks
    }, opts)}}`;
  }
}

class EndpointOptions {
  #inner;

  /**
   * @typedef {{
   *   retryTokenExpiration?: number,
   *   tokenExpiration?: number,
   *   maxWindowOverride?: number,
   *   maxStreamWindowOverride?: number,
   *   maxConnectionsPerHost?: number,
   *   maxConnectionsTotal?: number,
   *   maxStatelessResets?: number,
   *   addressLRUSize?: number,
   *   retryLimit?: number,
   *   maxPayloadSize?: number,
   *   unacknowledgedPacketThreshold?: number,
   *   validateAddress?: boolean,
   *   disableStatelessReset?: boolean,
   *   rxPacketLoss?: number,
   *   txPacketLoss?: number,
   *   ccAlgorithm?: CongestionControlAlgorithm,
   *   ipv6Only?: boolean,
   *   receiveBufferSize?: number,
   *   sendBufferSize?: number,
   *   ttl?: number,
   * }} EndpointOptions
   * @param {SocketAddress} address
   * @param {EndpointOptions} [options]
   */
  constructor(address, options = kEmptyObject) {
    validateSocketAddress(address, 'address');
    validateObject(options, 'options', options);

    const {
      retryTokenExpiration = DEFAULT_RETRYTOKEN_EXPIRATION,
      tokenExpiration = DEFAULT_TOKEN_EXPIRATION,
      maxWindowOverride = 0,
      maxStreamWindowOverride = 0,
      maxConnectionsPerHost = DEFAULT_MAX_CONNECTIONS_PER_HOST,
      maxConnectionsTotal = DEFAULT_MAX_CONNECTIONS,
      maxStatelessResets = DEFAULT_MAX_STATELESS_RESETS,
      addressLRUSize = DEFAULT_MAX_SOCKETADDRESS_LRU_SIZE,
      retryLimit = DEFAULT_MAX_RETRY_LIMIT,
      maxPayloadSize = 1200,
      unacknowledgedPacketThreshold = DEFAULT_UNACKNOWLEDGED_PACKET_THRESHOLD,
      validateAddress = true,
      disableStatelessReset = false,
      rxPacketLoss = 0.0,
      txPacketLoss = 0.0,
      ccAlgorithm = CongestionControlAlgorithm.CUBIC,
      ipv6Only = false,
      receiveBufferSize = 0,
      sendBufferSize = 0,
      ttl = 0,
    } = options;

    validateUint64(retryTokenExpiration, 'options.retryTokenExpiration');
    validateUint64(tokenExpiration, 'options.tokenExpiration');
    validateUint64(maxWindowOverride, 'options.maxWindowOverride');
    validateUint64(maxStreamWindowOverride, 'options.maxStreamWindowOverride');
    validateUint64(maxConnectionsPerHost, 'options.maxConnectionsPerHost');
    validateUint64(maxConnectionsTotal, 'options.maxConnectionsTotal');
    validateUint64(maxStatelessResets, 'options.maxStatelessResets');
    validateUint64(addressLRUSize, 'options.addressLRUSize');
    validateUint64(retryLimit, 'options.retryLimit');
    validateUint64(maxPayloadSize, 'options.maxPayloadSize');
    validateUint64(unacknowledgedPacketThreshold, 'options.unacknowledgedPacketThreshold');
    validateBoolean(validateAddress, 'options.validateAddress');
    validateBoolean(disableStatelessReset, 'options.disableStatelessReset');
    validateNumber(rxPacketLoss, 'options.rxPacketLoss', 0.0, 1.0);
    validateNumber(txPacketLoss, 'options.txPacketLoss', 0.0, 1.0);
    validateCongestionControlAlgorithm(ccAlgorithm, 'options.ccAlgorithm');
    validateBoolean(ipv6Only, 'options.ipv6Only');
    validateUint32(receiveBufferSize, 'options.receiveBufferSize');
    validateUint32(sendBufferSize, 'options.sendBufferSize');
    validateUint8(ttl, 'options.ttl');

    this.#inner = new EndpointOptions_(address[kSocketAddressHandle], {
      retryTokenExpiration,
      tokenExpiration,
      maxWindowOverride,
      maxStreamWindowOverride,
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResets,
      addressLRUSize,
      retryLimit,
      maxPayloadSize,
      unacknowledgedPacketThreshold,
      validateAddress,
      disableStatelessReset,
      rxPacketLoss,
      txPacketLoss,
      ccAlgorithm,
      ipv6Only,
      receiveBufferSize,
      sendBufferSize,
      ttl,
    });
  }

  /**
   * @returns {EndpointOptions}
   */
  generateResetTokenSecret() {
    this.#inner.generateResetTokenSecret();
    return this;
  }

  /**
   * @param {ArrayBufferView} secret - The new secret. Must be exactly 16 bytes.
   * @returns {EndpointOptions}
   */
  setResetTokenSecret(secret) {
    validateArrayBufferView(secret, 'secret');
    this.#inner.setResetTokenSecret(secret);
    return this;
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;
    return 'EndpointOptions {}';
  }

  [kCreateInstance]() {
    return createEndpoint(this.#inner);
  }
}

class Endpoint extends EventTarget {
  #inner;
  #closePromise;
  #stats;
  #state;
  #address;
  #closed = false;

  /**
   * @param {EndpointOptions} options
   */
  constructor(options) {
    super();
    validateEndpointOptions(options, 'options');
    this.#inner = options[kCreateInstance]();
    this.#inner[kOwner] = this;
    this.#state = new EndpointState(this.#inner.state);
    this.#stats = new EndpointStats(this.#inner.stats);
  }

  /**
   * Listen for server Sessions.
   * @param {SessionOptions} options The options to use for the Session.
   * @return {Endpoint}
   */
  listen(options) {
    if (this.#closed)
      throw new ERR_INVALID_STATE('The endpoint is closed');
    if (this.#closePromise !== undefined)
      throw new ERR_INVALID_STATE('The endpoint is closing');
    validateSessionOptions(options, 'options');
    options[kListen](this.#inner);
    return this;
  }

  /**
   * Create a client Session.
   * @param {SocketAddress} address The address to connect to.
   * @param {SessionOptions} options The options to use for the Session.
   * @returns {Session} The client session.
   */
  connect(address, options) {
    if (this.#closed)
      throw new ERR_INVALID_STATE('The endpoint is closed');
    if (this.#closePromise !== undefined)
      throw new ERR_INVALID_STATE('The endpoint is closing');
    validateSocketAddress(address, 'address');
    validateSessionOptions(options, 'options');
    return options[kCreateInstance](this.#inner, this, address);
  }

  /**
   * Close the Endpoint gracefully. Packets that are in flight are allowed to finish
   * and existing sessions are permitted to close. New sessions will not be allowed.
   * @async
   * @returns {Promise<void>} A promise resolved when the Endpoint is closed.
   */
  close() {
    if (this.#closePromise !== undefined)
      return this.#closePromise.promise;
    this.#closePromise = createDeferredPromise();
    this.#inner.closeGracefully();
    return this.#closePromise.promise;
  }

  /**
   * Statistics for this Endpoint.
   * @type {EndpointStats}
   */
  get stats() { return this.#stats; }

  /**
   * The current local address this endpoint is bound to.
   * @type {SocketAddress}
   */
  get address() {
    if (this.#address) return this.#address;

    const handle = this.#inner.address();
    if (handle !== undefined) {
      this.#address = new InternalSocketAddress(handle);
      return this.#address;
    }

    return undefined;
  }

  /**
   * @param {boolean} on = true
   * @returns {Endpoint}
   */
  markAsBusy(on = true) {
    if (this.#closed)
      throw new ERR_INVALID_STATE('The endpoint is closed');
    if (this.#closePromise !== undefined)
      throw new ERR_INVALID_STATE('The endpoint is closing');
    this.#inner.markBusy(on);
    return this;
  }

  /**
   * @returns {Endpoint}
   */
  ref() {
    if (this.#closed) return;
    this.#inner.ref();
    return this;
  }

  /**
   * @returns {Endpoint}
   */
  unref() {
    if (this.#closed) return;
    this.#inner.unref();
    return this;
  }

  [kFinishClose](maybeError) {
    this.#closed = true;
    this.#address = undefined;
    this.#stats[kDetach]();
    this.#state = undefined;

    // Were we waiting for a close? If so, resolve the promise.
    if (this.#closePromise !== undefined) {
      if (maybeError) {
        this.#closePromise.reject(maybeError);
      } else {
        this.#closePromise.resolve();
      }
    }

    this.#inner = undefined;

    this.dispatchEvent(new CloseEvent());

    if (maybeError)
      this.dispatchEvent(new ErrorEvent(maybeError));
  }

  [kNewSession](sessionHandle) {
    this.dispatchEvent(new SessionEvent(this, new Session(this, sessionHandle)));
  }

  [kInspect](depth, options) {
    if (depth < 0) return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1
    };

    return `Endpoint {${inspect({
      address: this.address,
      state: this.#state,
      stats: this.#stats,
      pendingClose: this.#closePromise !== undefined,
    }, opts)}}`;
  }
}

ObjectDefineProperties(Endpoint.prototype, {
  address: kEnumerableProperty,
  stats: kEnumerableProperty,
});
defineEventHandler(Endpoint.prototype, 'close');
defineEventHandler(Endpoint.prototype, 'error');
defineEventHandler(Endpoint.prototype, 'session');

// ======================================================================================
// Exports
module.exports = ObjectCreate(null, {
  constants: {
    __proto__: null,
    value: constants,
    enumerable: true,
    configurable: false,
  },
  EndpointOptions: {
    __proto__: null,
    value: EndpointOptions,
    enumerable: true,
    configurable: false,
  },
  SessionOptions: {
    __proto__: null,
    value: SessionOptions,
    enumerable: true,
    configurable: false,
  },
  Endpoint: {
    __proto__: null,
    value: Endpoint,
    enumerable: true,
    configurable: false,
  },
  ArrayBufferViewSource: {
    __proto__: null,
    value: ArrayBufferViewSource,
    enumerable: true,
    configurable: false,
  },
  StreamSource: {
    __proto__: null,
    value: StreamSource,
    enumerable: true,
    configurable: false,
  },
  StreamBaseSource: {
    __proto__: null,
    value: StreamBaseSource,
    enumerable: true,
    configurable: false,
  },
  BlobSource: {
    __proto__: null,
    value: BlobSource,
    enumerable: true,
    configurable: false,
  },
});

/* eslint-enable no-use-before-define */
