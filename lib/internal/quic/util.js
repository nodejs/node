'use strict';

const {
  Boolean,
  Number,
  NumberINFINITY,
  NumberNEGATIVE_INFINITY,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const {
  symbols: {
    owner_symbol,
  },
} = require('internal/async_hooks');

const { inspect } = require('internal/util/inspect');

const { Readable } = require('stream');
const {
  kHandle,
  kUpdateTimer,
  onStreamRead,
} = require('internal/stream_base_commons');

const endianness = require('os').endianness();

const assert = require('internal/assert');
assert(process.versions.ngtcp2 !== undefined);

const {
  isIP,
} = require('internal/net');

const {
  getOptionValue,
  getAllowUnauthorized,
} = require('internal/options');

const { Buffer } = require('buffer');

const {
  sessionConfig,
  http3Config,
  constants: {
    AF_INET,
    AF_INET6,
    NGHTTP3_ALPN_H3,
    DEFAULT_RETRYTOKEN_EXPIRATION,
    DEFAULT_MAX_CONNECTIONS,
    DEFAULT_MAX_CONNECTIONS_PER_HOST,
    DEFAULT_MAX_STATELESS_RESETS_PER_HOST,
    IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT,
    IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL,
    IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE,
    IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI,
    IDX_QUIC_SESSION_MAX_DATA,
    IDX_QUIC_SESSION_MAX_STREAMS_BIDI,
    IDX_QUIC_SESSION_MAX_STREAMS_UNI,
    IDX_QUIC_SESSION_MAX_IDLE_TIMEOUT,
    IDX_QUIC_SESSION_MAX_ACK_DELAY,
    IDX_QUIC_SESSION_MAX_UDP_PAYLOAD_SIZE,
    IDX_QUIC_SESSION_CC_ALGO,
    IDX_QUIC_SESSION_CONFIG_COUNT,

    IDX_QUICSESSION_STATE_KEYLOG_ENABLED,
    IDX_QUICSESSION_STATE_CLIENT_HELLO_ENABLED,
    IDX_QUICSESSION_STATE_OCSP_ENABLED,
    IDX_QUICSESSION_STATE_PATH_VALIDATED_ENABLED,
    IDX_QUICSESSION_STATE_USE_PREFERRED_ADDRESS_ENABLED,
    IDX_QUICSESSION_STATE_HANDSHAKE_CONFIRMED,
    IDX_QUICSESSION_STATE_IDLE_TIMEOUT,
    IDX_QUICSESSION_STATE_MAX_STREAMS_BIDI,
    IDX_QUICSESSION_STATE_MAX_STREAMS_UNI,
    IDX_QUICSESSION_STATE_MAX_DATA_LEFT,
    IDX_QUICSESSION_STATE_BYTES_IN_FLIGHT,

    IDX_QUICSOCKET_STATE_SERVER_LISTENING,
    IDX_QUICSOCKET_STATE_SERVER_BUSY,
    IDX_QUICSOCKET_STATE_STATELESS_RESET_DISABLED,

    IDX_QUICSTREAM_STATE_WRITE_ENDED,
    IDX_QUICSTREAM_STATE_READ_STARTED,
    IDX_QUICSTREAM_STATE_READ_PAUSED,
    IDX_QUICSTREAM_STATE_READ_ENDED,
    IDX_QUICSTREAM_STATE_FIN_SENT,
    IDX_QUICSTREAM_STATE_FIN_RECEIVED,

    IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY,
    IDX_HTTP3_QPACK_BLOCKED_STREAMS,
    IDX_HTTP3_MAX_HEADER_LIST_SIZE,
    IDX_HTTP3_MAX_PUSHES,
    IDX_HTTP3_MAX_HEADER_PAIRS,
    IDX_HTTP3_MAX_HEADER_LENGTH,
    IDX_HTTP3_CONFIG_COUNT,
    MAX_RETRYTOKEN_EXPIRATION,
    MIN_RETRYTOKEN_EXPIRATION,
    NGTCP2_NO_ERROR,
    NGTCP2_MAX_CIDLEN,
    NGTCP2_MIN_CIDLEN,
    NGTCP2_CC_ALGO_CUBIC,
    NGTCP2_CC_ALGO_RENO,
    QUIC_PREFERRED_ADDRESS_IGNORE,
    QUIC_PREFERRED_ADDRESS_USE,
    QUIC_ERROR_APPLICATION,
  }
} = internalBinding('quic');

const {
  validateBoolean,
  validateBuffer,
  validateInteger,
  validateObject,
  validatePort,
  validateString,
} = require('internal/validators');

const kDefaultQuicCiphers = 'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:' +
                            'TLS_CHACHA20_POLY1305_SHA256';
const kDefaultGroups = 'P-256:X25519:P-384:P-521';

let dns;

function lazyDNS() {
  if (!dns)
    dns = require('dns').promises;
  return dns;
}

// This is here rather than in internal/validators to
// prevent performance degredation in default cases.
function validateNumber(value, name,
                        min = NumberNEGATIVE_INFINITY,
                        max = NumberINFINITY) {
  if (typeof value !== 'number')
    throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
  if (value < min || value > max)
    throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
}

function getSocketType(type = 'udp4') {
  switch (type) {
    case 'udp4': return [AF_INET, false];
    case 'udp6': return [AF_INET6, false];
    case 'udp6-only': return [AF_INET6, true];
  }
  throw new ERR_INVALID_ARG_VALUE('options.type', type);
}

function defaultLookup(address, type) {
  const { lookup } = lazyDNS();
  return lookup(address || (type === 6 ? '::1' : '127.0.0.1'), type);
}

function validateCloseCode(code) {
  if (code != null && typeof code === 'object') {
    return {
      closeCode: code.code || NGTCP2_NO_ERROR,
      closeFamily: code.family || QUIC_ERROR_APPLICATION,
    };
  } else if (typeof code === 'number') {
    return {
      closeCode: code,
      closeFamily: QUIC_ERROR_APPLICATION,
    };
  }
  throw new ERR_INVALID_ARG_TYPE('code', ['number', 'Object'], code);
}

function validateLookup(lookup) {
  if (lookup && typeof lookup !== 'function')
    throw new ERR_INVALID_ARG_TYPE('options.lookup', 'function', lookup);
}

function validatePreferredAddress(address) {
  if (address !== undefined) {
    validateObject(address, 'options.preferredAddress');
    validateString(address.address, 'options.preferredAddress.address');
    if (address.port !== undefined)
      validatePort(address.port, 'options.preferredAddress.port');
    getSocketType(address.type);
  }
  return address;
}

// Validate known transport parameters, ignoring any that are not
// supported.  Ensures that only supported parameters are passed on.
function validateTransportParams(params) {
  const {
    activeConnectionIdLimit,
    maxStreamDataBidiLocal,
    maxStreamDataBidiRemote,
    maxStreamDataUni,
    maxData,
    maxStreamsBidi,
    maxStreamsUni,
    idleTimeout,
    maxUdpPayloadSize,
    maxAckDelay,
    preferredAddress,
    rejectUnauthorized,
    requestCert,
    congestionAlgorithm = 'reno',
    h3: {
      qpackMaxTableCapacity,
      qpackBlockedStreams,
      maxHeaderListSize,
      maxPushes,
      maxHeaderPairs,
      maxHeaderLength = getOptionValue('--max-http-header-size'),
    },
  } = { h3: {}, ...params };

  if (activeConnectionIdLimit !== undefined) {
    validateInteger(
      activeConnectionIdLimit,
      'options.activeConnectionIdLimit',
      /* min */ 2,
      /* max */ 8);
  }
  if (maxStreamDataBidiLocal !== undefined) {
    validateInteger(
      maxStreamDataBidiLocal,
      'options.maxStreamDataBidiLocal',
      /* min */ 0);
  }
  if (maxStreamDataBidiRemote !== undefined) {
    validateInteger(
      maxStreamDataBidiRemote,
      'options.maxStreamDataBidiRemote',
      /* min */ 0);
  }
  if (maxStreamDataUni !== undefined) {
    validateInteger(
      maxStreamDataUni,
      'options.maxStreamDataUni',
      /* min */ 0);
  }
  if (maxData !== undefined) {
    validateInteger(
      maxData,
      'options.maxData',
      /* min */ 0);
  }
  if (maxStreamsBidi !== undefined) {
    validateInteger(
      maxStreamsBidi,
      'options.maxStreamsBidi',
      /* min */ 0);
  }
  if (maxStreamsUni !== undefined) {
    validateInteger(
      maxStreamsUni,
      'options.maxStreamsUni',
      /* min */ 0);
  }
  if (idleTimeout !== undefined) {
    validateInteger(
      idleTimeout,
      'options.idleTimeout',
      /* min */ 0);
  }
  if (maxUdpPayloadSize !== undefined) {
    validateInteger(
      maxUdpPayloadSize,
      'options.maxUdpPayloadSize',
      /* min */ 0);
  }
  if (maxAckDelay !== undefined) {
    validateInteger(
      maxAckDelay,
      'options.maxAckDelay',
      /* min */ 0);
  }
  if (qpackMaxTableCapacity !== undefined) {
    validateInteger(
      qpackMaxTableCapacity,
      'options.h3.qpackMaxTableCapacity',
      /* min */ 0);
  }
  if (qpackBlockedStreams !== undefined) {
    validateInteger(
      qpackBlockedStreams,
      'options.h3.qpackBlockedStreams',
      /* min */ 0);
  }
  if (maxHeaderListSize !== undefined) {
    validateInteger(
      maxHeaderListSize,
      'options.h3.maxHeaderListSize',
      /* min */ 0);
  }
  if (maxPushes !== undefined) {
    validateInteger(
      maxPushes,
      'options.h3.maxPushes',
      /* min */ 0);
  }
  if (maxHeaderPairs !== undefined) {
    validateInteger(
      maxHeaderPairs,
      'options.h3.maxHeaderPairs',
      /* min */ 0);
  }
  if (maxHeaderLength !== undefined) {
    validateInteger(
      maxHeaderLength,
      'options.h3.maxHeaderLength',
      /* min */ 0);
  }
  let ccAlgo = NGTCP2_CC_ALGO_RENO;
  if (congestionAlgorithm !== undefined) {
    validateString(congestionAlgorithm, 'options.conjestionAlgorithm');
    switch (congestionAlgorithm) {
      case 'reno':
        // This is the default
        break;
      case 'cubic':
        ccAlgo = NGTCP2_CC_ALGO_CUBIC;
        break;
      default:
        throw new ERR_INVALID_ARG_VALUE(
          'options.congestionAlgorithm',
          congestionAlgorithm,
          'is not supported');
    }
  }

  validatePreferredAddress(preferredAddress);

  return {
    activeConnectionIdLimit,
    maxStreamDataBidiLocal,
    maxStreamDataBidiRemote,
    maxStreamDataUni,
    maxData,
    maxStreamsBidi,
    maxStreamsUni,
    idleTimeout,
    maxUdpPayloadSize,
    maxAckDelay,
    preferredAddress,
    rejectUnauthorized,
    requestCert,
    congestionAlgorithm: ccAlgo,
    h3: {
      qpackMaxTableCapacity,
      qpackBlockedStreams,
      maxHeaderListSize,
      maxPushes,
      maxHeaderPairs,
      maxHeaderLength,
    }
  };
}

function validateQuicClientSessionOptions(options = {}) {
  const {
    autoStart = true,
    address = 'localhost',
    alpn = '',
    dcid: dcid_value,
    minDHSize = 1024,
    ocspHandler,
    port = 0,
    preferredAddressPolicy = 'ignore',
    remoteTransportParams,
    servername = (isIP(address) ? '' : address),
    sessionTicket,
    verifyHostnameIdentity = true,
    qlog = false,
    highWaterMark,
    defaultEncoding,
  } = options;

  validateBoolean(autoStart, 'options.autoStart');
  validateNumber(minDHSize, 'options.minDHSize');
  validatePort(port, 'options.port');
  validateString(address, 'options.address');
  validateString(alpn, 'options.alpn');
  validateString(servername, 'options.servername');

  if (isIP(servername)) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.servername',
      servername,
      'cannot be an IP address');
  }

  if (remoteTransportParams !== undefined)
    validateBuffer(remoteTransportParams, 'options.remoteTransportParams');

  if (sessionTicket !== undefined)
    validateBuffer(sessionTicket, 'options.sessionTicket');

  let dcid;
  if (dcid_value !== undefined) {
    if (typeof dcid_value === 'string') {
      // If it's a string, it must be a hex encoded string
      try {
        dcid = Buffer.from(dcid_value, 'hex');
      } catch {
        throw new ERR_INVALID_ARG_VALUE(
          'options.dcid',
          dcid,
          'is not a valid QUIC connection ID');
      }
    }

    validateBuffer(
      dcid_value,
      'options.dcid',
      ['string', 'Buffer', 'TypedArray', 'DataView']);

    if (dcid_value.length > NGTCP2_MAX_CIDLEN ||
        dcid_value.length < NGTCP2_MIN_CIDLEN) {
      throw new ERR_INVALID_ARG_VALUE(
        'options.dcid',
        dcid_value.toString('hex'),
        'is not a valid QUIC connection ID'
      );
    }

    dcid = dcid_value;
  }

  if (preferredAddressPolicy !== undefined)
    validateString(preferredAddressPolicy, 'options.preferredAddressPolicy');

  validateBoolean(verifyHostnameIdentity, 'options.verifyHostnameIdentity');
  validateBoolean(qlog, 'options.qlog');

  if (ocspHandler !== undefined && typeof ocspHandler !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.ocspHandler',
      'functon',
      ocspHandler);
  }

  return {
    autoStart,
    address,
    alpn,
    dcid,
    minDHSize,
    ocspHandler,
    port,
    preferredAddressPolicy:
      preferredAddressPolicy === 'accept' ?
        QUIC_PREFERRED_ADDRESS_USE :
        QUIC_PREFERRED_ADDRESS_IGNORE,
    remoteTransportParams,
    servername,
    sessionTicket,
    verifyHostnameIdentity,
    qlog,
    ...validateQuicStreamOptions({ highWaterMark, defaultEncoding })
  };
}

function validateQuicStreamOptions(options = {}) {
  validateObject(options);
  const {
    defaultEncoding = 'utf8',
    halfOpen,
    highWaterMark,
  } = options;
  if (!Buffer.isEncoding(defaultEncoding)) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.defaultEncoding',
      defaultEncoding,
      'is not a valid encoding');
  }
  if (halfOpen !== undefined)
    validateBoolean(halfOpen, 'options.halfOpen');
  if (highWaterMark !== undefined) {
    validateInteger(
      highWaterMark,
      'options.highWaterMark',
      /* min */ 0);
  }
  return {
    defaultEncoding,
    halfOpen,
    highWaterMark,
  };
}

function validateQuicEndpointOptions(options = {}, name = 'options') {
  validateObject(options, name);
  if (options === null || typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  const {
    address,
    lookup,
    port = 0,
    reuseAddr = false,
    type = 'udp4',
    preferred = false,
  } = options;
  if (address !== undefined)
    validateString(address, 'options.address');
  validatePort(port, 'options.port');
  validateString(type, 'options.type');
  validateLookup(lookup);
  validateBoolean(reuseAddr, 'options.reuseAddr');
  validateBoolean(preferred, 'options.preferred');
  const [typeVal, ipv6Only] = getSocketType(type);
  return {
    type: typeVal,
    ipv6Only,
    address,
    lookup,
    port,
    preferred,
    reuseAddr,
  };
}

function validateQuicSocketOptions(options = {}) {
  validateObject(options, 'options');

  const {
    client = {},
    disableStatelessReset = false,
    endpoint = { port: 0, type: 'udp4' },
    lookup = defaultLookup,
    maxConnections = DEFAULT_MAX_CONNECTIONS,
    maxConnectionsPerHost = DEFAULT_MAX_CONNECTIONS_PER_HOST,
    maxStatelessResetsPerHost = DEFAULT_MAX_STATELESS_RESETS_PER_HOST,
    qlog = false,
    retryTokenTimeout = DEFAULT_RETRYTOKEN_EXPIRATION,
    server = {},
    statelessResetSecret,
    validateAddress = false,
  } = options;

  const { type } = validateQuicEndpointOptions(endpoint, 'options.endpoint');
  validateObject(client, 'options.client');
  validateObject(server, 'options.server');
  validateLookup(lookup);
  validateBoolean(validateAddress, 'options.validateAddress');
  validateBoolean(qlog, 'options.qlog');
  validateBoolean(disableStatelessReset, 'options.disableStatelessReset');

  if (retryTokenTimeout !== undefined) {
    validateInteger(
      retryTokenTimeout,
      'options.retryTokenTimeout',
      /* min */ MIN_RETRYTOKEN_EXPIRATION,
      /* max */ MAX_RETRYTOKEN_EXPIRATION);
  }
  if (maxConnections !== undefined) {
    validateInteger(
      maxConnections,
      'options.maxConnections',
      /* min */ 1);
  }
  if (maxConnectionsPerHost !== undefined) {
    validateInteger(
      maxConnectionsPerHost,
      'options.maxConnectionsPerHost',
      /* min */ 1);
  }
  if (maxStatelessResetsPerHost !== undefined) {
    validateInteger(
      maxStatelessResetsPerHost,
      'options.maxStatelessResetsPerHost',
      /* min */ 1);
  }

  if (statelessResetSecret !== undefined) {
    validateBuffer(statelessResetSecret, 'options.statelessResetSecret');
    if (statelessResetSecret.length !== 16)
      throw new ERR_INVALID_ARG_VALUE(
        'options.statelessResetSecret',
        statelessResetSecret,
        'must be exactly 16 bytes in length');
  }

  return {
    endpoint,
    client,
    lookup,
    maxConnections,
    maxConnectionsPerHost,
    maxStatelessResetsPerHost,
    retryTokenTimeout,
    server,
    type,
    validateAddress,
    qlog,
    statelessResetSecret,
    disableStatelessReset,
  };
}

function validateQuicSocketListenOptions(options = {}) {
  validateObject(options);
  const {
    alpn = NGHTTP3_ALPN_H3,
    defaultEncoding,
    highWaterMark,
    ocspHandler,
    clientHelloHandler,
    requestCert,
    rejectUnauthorized,
    lookup,
  } = options;
  validateString(alpn, 'options.alpn');
  if (rejectUnauthorized !== undefined)
    validateBoolean(rejectUnauthorized, 'options.rejectUnauthorized');
  if (requestCert !== undefined)
    validateBoolean(requestCert, 'options.requestCert');
  if (lookup !== undefined)
    validateLookup(lookup);
  if (ocspHandler !== undefined && typeof ocspHandler !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.ocspHandler',
      'function',
      ocspHandler);
  }
  if (clientHelloHandler !== undefined &&
      typeof clientHelloHandler !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.clientHelloHandler',
      'function',
      clientHelloHandler);
  }
  const transportParams =
    validateTransportParams(
      options,
      NGTCP2_MAX_CIDLEN,
      NGTCP2_MIN_CIDLEN);

  return {
    alpn,
    lookup,
    ocspHandler,
    clientHelloHandler,
    rejectUnauthorized,
    requestCert,
    transportParams,
    ...validateQuicStreamOptions({ highWaterMark, defaultEncoding })
  };
}

function validateQuicSocketConnectOptions(options = {}) {
  validateObject(options);
  const {
    type = 'udp4',
    address,
    lookup,
  } = options;
  if (address !== undefined)
    validateString(address, 'options.address');
  if (lookup !== undefined)
    validateLookup(lookup);
  const [typeVal] = getSocketType(type);
  return { type: typeVal, address, lookup };
}

function validateCreateSecureContextOptions(options = {}) {
  validateObject(options);
  const {
    ca,
    cert,
    ciphers = kDefaultQuicCiphers,
    clientCertEngine,
    crl,
    dhparam,
    ecdhCurve,
    groups = kDefaultGroups,
    honorCipherOrder,
    key,
    earlyData = true,  // Early data is enabled by default
    passphrase,
    pfx,
    sessionIdContext,
    secureProtocol
  } = options;
  validateString(ciphers, 'option.ciphers');
  validateString(groups, 'option.groups');
  if (earlyData !== undefined)
    validateBoolean(earlyData, 'option.earlyData');

  // Additional validation occurs within the tls
  // createSecureContext function.
  return {
    ca,
    cert,
    ciphers,
    clientCertEngine,
    crl,
    dhparam,
    ecdhCurve,
    groups,
    honorCipherOrder,
    key,
    earlyData,
    passphrase,
    pfx,
    sessionIdContext,
    secureProtocol
  };
}

function setConfigField(buffer, val, index) {
  if (typeof val === 'number') {
    buffer[index] = val;
    return 1 << index;
  }
  return 0;
}

// Extracts configuration options and updates the aliased buffer
// arrays that are used to communicate config choices to the c++
// internals.
function setTransportParams(config) {
  const {
    activeConnectionIdLimit,
    maxStreamDataBidiLocal,
    maxStreamDataBidiRemote,
    maxStreamDataUni,
    maxData,
    maxStreamsBidi,
    maxStreamsUni,
    idleTimeout,
    maxUdpPayloadSize,
    maxAckDelay,
    congestionAlgorithm,
    h3: {
      qpackMaxTableCapacity,
      qpackBlockedStreams,
      maxHeaderListSize,
      maxPushes,
      maxHeaderPairs,
      maxHeaderLength,
    },
  } = { h3: {}, ...config };

  // The const flags is a bitmap that is used to communicate whether or not a
  // given configuration value has been explicitly provided.
  const flags = setConfigField(sessionConfig,
                               activeConnectionIdLimit,
                               IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT) |
                setConfigField(sessionConfig,
                               maxStreamDataBidiLocal,
                               IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL) |
                setConfigField(sessionConfig,
                               maxStreamDataBidiRemote,
                               IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE) |
                setConfigField(sessionConfig,
                               maxStreamDataUni,
                               IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI) |
                setConfigField(sessionConfig,
                               maxData,
                               IDX_QUIC_SESSION_MAX_DATA) |
                setConfigField(sessionConfig,
                               maxStreamsBidi,
                               IDX_QUIC_SESSION_MAX_STREAMS_BIDI) |
                setConfigField(sessionConfig,
                               maxStreamsUni,
                               IDX_QUIC_SESSION_MAX_STREAMS_UNI) |
                setConfigField(sessionConfig,
                               idleTimeout,
                               IDX_QUIC_SESSION_MAX_IDLE_TIMEOUT) |
                setConfigField(sessionConfig,
                               maxAckDelay,
                               IDX_QUIC_SESSION_MAX_ACK_DELAY) |
                setConfigField(sessionConfig,
                               maxUdpPayloadSize,
                               IDX_QUIC_SESSION_MAX_UDP_PAYLOAD_SIZE) |
                setConfigField(sessionConfig,
                               congestionAlgorithm,
                               IDX_QUIC_SESSION_CC_ALGO);

  sessionConfig[IDX_QUIC_SESSION_CONFIG_COUNT] = flags;

  const h3flags = setConfigField(http3Config,
                                 qpackMaxTableCapacity,
                                 IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY) |
                  setConfigField(http3Config,
                                 qpackBlockedStreams,
                                 IDX_HTTP3_QPACK_BLOCKED_STREAMS) |
                  setConfigField(http3Config,
                                 maxHeaderListSize,
                                 IDX_HTTP3_MAX_HEADER_LIST_SIZE) |
                  setConfigField(http3Config,
                                 maxPushes,
                                 IDX_HTTP3_MAX_PUSHES) |
                  setConfigField(http3Config,
                                 maxHeaderPairs,
                                 IDX_HTTP3_MAX_HEADER_PAIRS) |
                  setConfigField(http3Config,
                                 maxHeaderLength,
                                 IDX_HTTP3_MAX_HEADER_LENGTH);

  http3Config[IDX_HTTP3_CONFIG_COUNT] = h3flags;
}

// Some events that are emitted originate from the C++ internals and are
// fairly expensive and optional. An aliased array buffer is used to
// communicate that a handler has been added for the optional events
// so that the C++ internals know there is an actual listener. The event
// will not be emitted if there is no handler.
function toggleListeners(state, event, on) {
  if (state === undefined)
    return;
  switch (event) {
    case 'keylog':
      state.keylogEnabled = on;
      break;
    case 'pathValidation':
      state.pathValidatedEnabled = on;
      break;
    case 'usePreferredAddress':
      state.usePreferredAddressEnabled = on;
      break;
  }
}

class QuicStreamSharedState {
  constructor(state) {
    this[kHandle] = Buffer.from(state);
  }

  get writeEnded() {
    return Boolean(this[kHandle].readUInt8(IDX_QUICSTREAM_STATE_WRITE_ENDED));
  }

  set writeEnded(on) {
    this[kHandle].writeUInt8(on ? 1 : 0, IDX_QUICSTREAM_STATE_WRITE_ENDED);
  }

  get readStarted() {
    return Boolean(this[kHandle].readUInt8(IDX_QUICSTREAM_STATE_READ_STARTED));
  }

  get readPaused() {
    return Boolean(this[kHandle].readUInt8(IDX_QUICSTREAM_STATE_READ_PAUSED));
  }

  set readPaused(on) {
    this[kHandle].writeUInt8(on ? 1 : 0, IDX_QUICSTREAM_STATE_READ_PAUSED);
  }

  get readEnded() {
    return Boolean(this[kHandle].readUInt8(IDX_QUICSTREAM_STATE_READ_ENDED));
  }

  set readEnded(on) {
    this[kHandle].writeUInt8(on ? 1 : 0, IDX_QUICSTREAM_STATE_READ_ENDED);
  }

  get finSent() {
    return Boolean(this[kHandle].readUInt8(IDX_QUICSTREAM_STATE_FIN_SENT));
  }

  get finReceived() {
    return Boolean(this[kHandle].readUInt8(IDX_QUICSTREAM_STATE_FIN_RECEIVED));
  }
}

class QuicSocketSharedState {
  constructor(state) {
    this[kHandle] = Buffer.from(state);
  }

  get serverListening() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSOCKET_STATE_SERVER_LISTENING));
  }

  set serverListening(on) {
    this[kHandle].writeUInt8(on ? 1 : 0, IDX_QUICSOCKET_STATE_SERVER_LISTENING);
  }

  get serverBusy() {
    return Boolean(this[kHandle].readUInt8(IDX_QUICSOCKET_STATE_SERVER_BUSY));
  }

  set serverBusy(on) {
    this[kHandle].writeUInt8(on ? 1 : 0, IDX_QUICSOCKET_STATE_SERVER_BUSY);
  }

  get statelessResetDisabled() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSOCKET_STATE_STATELESS_RESET_DISABLED));
  }

  set statelessResetDisabled(on) {
    this[kHandle].writeUInt8(on ? 1 : 0,
                             IDX_QUICSOCKET_STATE_STATELESS_RESET_DISABLED);
  }
}

// A utility class used to handle reading / modifying shared JS/C++
// state associated with a QuicSession
class QuicSessionSharedState {
  constructor(state) {
    this[kHandle] = Buffer.from(state);
  }

  get keylogEnabled() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSESSION_STATE_KEYLOG_ENABLED));
  }

  set keylogEnabled(on) {
    this[kHandle]
      .writeUInt8(on ? 1 : 0, IDX_QUICSESSION_STATE_KEYLOG_ENABLED);
  }

  get clientHelloEnabled() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSESSION_STATE_CLIENT_HELLO_ENABLED));
  }

  set clientHelloEnabled(on) {
    this[kHandle]
      .writeUInt8(on ? 1 : 0, IDX_QUICSESSION_STATE_CLIENT_HELLO_ENABLED);
  }

  get ocspEnabled() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSESSION_STATE_OCSP_ENABLED));
  }

  set ocspEnabled(on) {
    this[kHandle]
      .writeUInt8(on ? 1 : 0, IDX_QUICSESSION_STATE_OCSP_ENABLED);
  }

  get pathValidatedEnabled() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSESSION_STATE_PATH_VALIDATED_ENABLED));
  }

  set pathValidatedEnabled(on) {
    this[kHandle]
      .writeUInt8(on ? 1 : 0, IDX_QUICSESSION_STATE_PATH_VALIDATED_ENABLED);
  }

  get usePreferredAddressEnabled() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSESSION_STATE_USE_PREFERRED_ADDRESS_ENABLED));
  }

  set usePreferredAddressEnabled(on) {
    this[kHandle]
      .writeUInt8(on ? 1 : 0,
                  IDX_QUICSESSION_STATE_USE_PREFERRED_ADDRESS_ENABLED);
  }

  get handshakeConfirmed() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSESSION_STATE_HANDSHAKE_CONFIRMED));
  }

  get idleTimeout() {
    return Boolean(this[kHandle]
      .readUInt8(IDX_QUICSESSION_STATE_IDLE_TIMEOUT));
  }

  get maxStreamsBidi() {
    return Number(endianness === 'BE' ?
      this[kHandle].readBigInt64BE(IDX_QUICSESSION_STATE_MAX_STREAMS_BIDI) :
      this[kHandle].readBigInt64LE(IDX_QUICSESSION_STATE_MAX_STREAMS_BIDI));
  }

  get maxStreamsUni() {
    return Number(endianness === 'BE' ?
      this[kHandle].readBigInt64BE(IDX_QUICSESSION_STATE_MAX_STREAMS_UNI) :
      this[kHandle].readBigInt64LE(IDX_QUICSESSION_STATE_MAX_STREAMS_UNI));
  }

  get maxDataLeft() {
    return Number(endianness === 'BE' ?
      this[kHandle].readBigInt64BE(IDX_QUICSESSION_STATE_MAX_DATA_LEFT) :
      this[kHandle].readBigInt64LE(IDX_QUICSESSION_STATE_MAX_DATA_LEFT));
  }

  get bytesInFlight() {
    return Number(endianness === 'BE' ?
      this[kHandle].readBigInt64BE(IDX_QUICSESSION_STATE_BYTES_IN_FLIGHT) :
      this[kHandle].readBigInt64LE(IDX_QUICSESSION_STATE_BYTES_IN_FLIGHT));
  }
}

class QLogStream extends Readable {
  constructor(handle) {
    super({ autoDestroy: true });
    this[kHandle] = handle;
    handle[owner_symbol] = this;
    handle.onread = onStreamRead;
  }

  _read() {
    if (this[kHandle])
      this[kHandle].readStart();
  }

  _destroy() {
    // Release the references on the handle so that
    // it can be garbage collected.
    this[kHandle][owner_symbol] = undefined;
    this[kHandle] = undefined;
  }

  [kUpdateTimer]() {
    // Does nothing
  }
}

function customInspect(self, obj, depth, options) {
  if (depth < 0)
    return self;

  const opts = {
    ...options,
    depth: options.depth == null ? null : options.depth - 1
  };

  return `${self.constructor.name} ${inspect(obj, opts)}`;
}

module.exports = {
  customInspect,
  getAllowUnauthorized,
  getSocketType,
  defaultLookup,
  setTransportParams,
  toggleListeners,
  validateNumber,
  validateCloseCode,
  validateTransportParams,
  validateQuicClientSessionOptions,
  validateQuicSocketOptions,
  validateQuicStreamOptions,
  validateQuicSocketListenOptions,
  validateQuicEndpointOptions,
  validateCreateSecureContextOptions,
  validateQuicSocketConnectOptions,
  QuicStreamSharedState,
  QuicSocketSharedState,
  QuicSessionSharedState,
  QLogStream,
};
