'use strict';

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
    ERR_QUICSESSION_INVALID_DCID,
    ERR_QUICSOCKET_INVALID_STATELESS_RESET_SECRET_LENGTH,
  },
} = require('internal/errors');

const { Number } = primordials;

const { Buffer } = require('buffer');
const { isArrayBufferView } = require('internal/util/types');
const {
  isLegalPort,
  isIP,
} = require('internal/net');

const { kHandle } = require('internal/stream_base_commons');

const {
  sessionConfig,
  http3Config,
  constants: {
    AF_INET,
    AF_INET6,
    DEFAULT_RETRYTOKEN_EXPIRATION,
    DEFAULT_MAX_CONNECTIONS_PER_HOST,
    IDX_QUIC_SESSION_ACTIVE_CONNECTION_ID_LIMIT,
    IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_LOCAL,
    IDX_QUIC_SESSION_MAX_STREAM_DATA_BIDI_REMOTE,
    IDX_QUIC_SESSION_MAX_STREAM_DATA_UNI,
    IDX_QUIC_SESSION_MAX_DATA,
    IDX_QUIC_SESSION_MAX_STREAMS_BIDI,
    IDX_QUIC_SESSION_MAX_STREAMS_UNI,
    IDX_QUIC_SESSION_IDLE_TIMEOUT,
    IDX_QUIC_SESSION_MAX_ACK_DELAY,
    IDX_QUIC_SESSION_MAX_PACKET_SIZE,
    IDX_QUIC_SESSION_CONFIG_COUNT,
    IDX_QUIC_SESSION_STATE_CERT_ENABLED,
    IDX_QUIC_SESSION_STATE_CLIENT_HELLO_ENABLED,
    IDX_QUIC_SESSION_STATE_KEYLOG_ENABLED,
    IDX_QUIC_SESSION_STATE_PATH_VALIDATED_ENABLED,
    IDX_HTTP3_QPACK_MAX_TABLE_CAPACITY,
    IDX_HTTP3_QPACK_BLOCKED_STREAMS,
    IDX_HTTP3_MAX_HEADER_LIST_SIZE,
    IDX_HTTP3_MAX_PUSHES,
    IDX_HTTP3_CONFIG_COUNT,
    MAX_RETRYTOKEN_EXPIRATION,
    MIN_RETRYTOKEN_EXPIRATION,
    NGTCP2_NO_ERROR,
    NGTCP2_MAX_CIDLEN,
    NGTCP2_MIN_CIDLEN,
    QUIC_PREFERRED_ADDRESS_IGNORE,
    QUIC_PREFERRED_ADDRESS_ACCEPT,
    QUIC_ERROR_APPLICATION,
  }
} = internalBinding('quic');

let warnOnAllowUnauthorized = true;
let dns;

function lazyDNS() {
  if (!dns)
    dns = require('dns');
  return dns;
}

function getAllowUnauthorized() {
  const allowUnauthorized = process.env.NODE_TLS_REJECT_UNAUTHORIZED === '0';

  if (allowUnauthorized && warnOnAllowUnauthorized) {
    warnOnAllowUnauthorized = false;
    process.emitWarning(
      'Setting the NODE_TLS_REJECT_UNAUTHORIZED ' +
      'environment variable to \'0\' makes TLS connections ' +
      'and HTTPS requests insecure by disabling ' +
      'certificate verification.');
  }
  return allowUnauthorized;
}

function getSocketType(type) {
  switch (type) {
    case 'udp4': return AF_INET;
    case 'udp6': return AF_INET6;
  }
  throw new ERR_INVALID_ARG_VALUE('options.type', type);
}

function lookup4(address, callback) {
  const { lookup } = lazyDNS();
  lookup(address || '127.0.0.1', 4, callback);
}

function lookup6(address, callback) {
  const { lookup } = lazyDNS();
  lookup(address || '::1', 6, callback);
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

function validateNumberInRange(val, name, range) {
  if (val === undefined)
    return;
  if (!Number.isSafeInteger(val))
    throw new ERR_INVALID_ARG_TYPE(name, 'safe integer', val);
  if (val < 0)
    throw new ERR_OUT_OF_RANGE(name, range, val);
}

function validateNumberInBoundedRange(val, name, min, max) {
  if (val === undefined)
    return;
  if (!Number.isSafeInteger(val))
    throw new ERR_INVALID_ARG_TYPE(name, 'safe integer', val);
  if (val < min || val > max)
    throw new ERR_OUT_OF_RANGE(name, `${min} <= ${name} <= ${max}`, val);
}

function validateBoolean(val, name) {
  if (typeof val !== 'boolean')
    throw new ERR_INVALID_ARG_TYPE(name, 'boolean', val);
}

function validateString(val, name) {
  if (typeof val !== 'string')
    throw new ERR_INVALID_ARG_TYPE(name, 'string', val);
}

function validateLookup(lookup) {
  if (lookup && typeof lookup !== 'function')
    throw new ERR_INVALID_ARG_TYPE('options.lookup', 'Function', lookup);
}

function validatePort(port) {
  if (!isLegalPort(port)) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.port',
      port,
      'is not a valid IP port');
  }
}

function validateArrayBufferView(
  val,
  name,
  types = ['Buffer', 'TypedArray', 'DataView']) {
  if (!isArrayBufferView(val))
    throw new ERR_INVALID_ARG_TYPE(name, types, val);
}

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
    maxPacketSize,
    maxAckDelay,
    preferredAddress,
    rejectUnauthorized,
    requestCert,
    h3: {
      qpackMaxTableCapacity,
      qpackBlockedStreams,
      maxHeaderListSize,
      maxPushes,
    },
  } = { h3: {}, ...params };

  validateNumberInBoundedRange(
    activeConnectionIdLimit,
    'options.activeConnectionIdLimit',
    10,
    Number.MAX_SAFE_INTEGER);
  validateNumberInRange(
    maxStreamDataBidiLocal,
    'options.maxStreamDataBidiLocal',
    '>=0');
  validateNumberInRange(
    maxStreamDataBidiRemote,
    'options.maxStreamDataBidiRemote',
    '>=0');
  validateNumberInRange(
    maxStreamDataUni,
    'options.maxStreamDataUni',
    '>=0');
  validateNumberInRange(
    maxData,
    'options.maxData',
    '>=0');
  validateNumberInRange(
    maxStreamsBidi,
    'options.maxStreamsBidi',
    '>=0');
  validateNumberInRange(
    maxStreamsUni,
    'options.maxStreamsUni',
    '>=0');
  validateNumberInRange(
    idleTimeout,
    'options.idleTimeout',
    '>=0');
  validateNumberInRange(
    maxPacketSize,
    'options.maxPacketSize',
    '>=0');
  validateNumberInRange(
    maxAckDelay,
    'options.maxAckDelay',
    '>=0');
  validateNumberInRange(
    qpackMaxTableCapacity,
    'options.qpackMaxTableCapacity',
    '>=0');
  validateNumberInRange(
    qpackBlockedStreams,
    'options.qpackBlockedStreams',
    '>=0');
  validateNumberInRange(
    maxHeaderListSize,
    'options.maxHeaderListSize',
    '>=0');
  validateNumberInRange(
    maxPushes,
    'options.maxPushes',
    '>=0');

  return {
    activeConnectionIdLimit,
    maxStreamDataBidiLocal,
    maxStreamDataBidiRemote,
    maxStreamDataUni,
    maxData,
    maxStreamsBidi,
    maxStreamsUni,
    idleTimeout,
    maxPacketSize,
    maxAckDelay,
    preferredAddress,
    rejectUnauthorized,
    requestCert,
    h3: {
      qpackMaxTableCapacity,
      qpackBlockedStreams,
      maxHeaderListSize,
      maxPushes,
    }
  };
}

function validateQuicClientSessionOptions(options = {}) {
  if (options !== null && typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  const {
    address,
    alpn = '',
    dcid: dcid_value,
    ipv6Only = false,
    minDHSize = 1024,
    port = 0,
    preferredAddressPolicy = 'ignore',
    remoteTransportParams,
    requestOCSP = false,
    servername = address,
    sessionTicket,
    verifyHostnameIdentity = true,
    qlog = false,
  } = options;

  if (typeof minDHSize !== 'number') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.minDHSize', 'number', minDHSize);
  }

  validatePort(port);
  validateString(servername, 'options.servername');
  validateString(alpn, 'options.alpn');

  if (isIP(servername)) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.servername',
      servername,
      'cannot be an IP address');
  }

  if (remoteTransportParams) {
    validateArrayBufferView(
      remoteTransportParams,
      'options.remoteTransportParams');
  }

  if (sessionTicket) {
    validateArrayBufferView(
      sessionTicket,
      'options.sessionTicket');
  }

  let dcid;
  if (dcid_value !== undefined) {
    if (typeof dcid_value === 'string') {
      // If it's a string, it must be a hex encoded string
      try {
        dcid = Buffer.from(dcid_value, 'hex');
      } catch {
        throw new ERR_QUICSESSION_INVALID_DCID(dcid);
      }
    }

    validateArrayBufferView(
      dcid_value,
      'options.dcid',
      ['string', 'Buffer', 'TypedArray', 'DataView']);

    if (dcid_value.length > NGTCP2_MAX_CIDLEN ||
        dcid_value.length < NGTCP2_MIN_CIDLEN) {
      throw new ERR_QUICSESSION_INVALID_DCID(dcid_value.toString('hex'));
    }

    dcid = dcid_value;
  }

  if (preferredAddressPolicy !== undefined)
    validateString(preferredAddressPolicy, 'options.preferredAddressPolicy');

  validateBoolean(requestOCSP, 'options.requestOCSP');
  validateBoolean(verifyHostnameIdentity, 'options.verifyHostnameIdentity');
  validateBoolean(qlog, 'options.qlog');

  return {
    address,
    alpn,
    dcid,
    ipv6Only,
    minDHSize,
    port,
    preferredAddressPolicy:
      preferredAddressPolicy === 'accept' ?
        QUIC_PREFERRED_ADDRESS_ACCEPT :
        QUIC_PREFERRED_ADDRESS_IGNORE,
    remoteTransportParams,
    requestOCSP,
    servername,
    sessionTicket,
    verifyHostnameIdentity,
    qlog,
  };
}

function validateQuicEndpointOptions(options = {}) {
  if (options === null || typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
  const {
    address,
    ipv6Only = false,
    lookup,
    port = 0,
    reuseAddr = false,
    type = 'udp4',
    preferred = false,
  } = options;
  if (address != null)
    validateString(address, 'options.address');
  validatePort(port);
  validateString(type, 'options.type');
  validateLookup(lookup);
  validateBoolean(ipv6Only, 'options.ipv6Only');
  validateBoolean(reuseAddr, 'options.reuseAddr');
  validateBoolean(preferred, 'options.preferred');
  return {
    address,
    ipv6Only,
    lookup,
    port,
    preferred,
    reuseAddr,
    type: getSocketType(type),
  };
}

function validateQuicSocketOptions(options = {}) {
  if (options === null || typeof options !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);

  const {
    autoClose = false,
    client,
    endpoint = { port: 0, type: 'udp4' },
    lookup,
    maxConnectionsPerHost = DEFAULT_MAX_CONNECTIONS_PER_HOST,
    qlog = false,
    retryTokenTimeout = DEFAULT_RETRYTOKEN_EXPIRATION,
    server,
    statelessResetSecret,
    type = endpoint.type || 'udp4',
    validateAddressLRU = false,
    validateAddress = false,
  } = options;

  if (endpoint === null || typeof endpoint !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options.endpoint', 'Object', endpoint);

  validateString(type, 'options.type');
  validateLookup(lookup);
  validateBoolean(validateAddress, 'options.validateAddress');
  validateBoolean(validateAddressLRU, 'options.validateAddressLRU');
  validateBoolean(autoClose, 'options.autoClose');
  validateBoolean(qlog, 'options.qlog');

  validateNumberInBoundedRange(
    retryTokenTimeout,
    'options.retryTokenTimeout',
    MIN_RETRYTOKEN_EXPIRATION,
    MAX_RETRYTOKEN_EXPIRATION);
  validateNumberInBoundedRange(
    maxConnectionsPerHost,
    'options.maxConnectionsPerHost',
    1, Number.MAX_SAFE_INTEGER);

  if (statelessResetSecret) {
    if (!isArrayBufferView(statelessResetSecret)) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.statelessResetSecret',
        ['string', 'Buffer', 'TypedArray', 'DataView'],
        statelessResetSecret);
    }
    if (statelessResetSecret.length !== 16)
      throw new ERR_QUICSOCKET_INVALID_STATELESS_RESET_SECRET_LENGTH();
  }

  return {
    endpoint,
    autoClose,
    client,
    lookup,
    maxConnectionsPerHost,
    retryTokenTimeout,
    server,
    type: getSocketType(type),
    validateAddress: validateAddress || validateAddressLRU,
    validateAddressLRU,
    qlog,
    statelessResetSecret,
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
    maxPacketSize,
    maxAckDelay,
    h3: {
      qpackMaxTableCapacity,
      qpackBlockedStreams,
      maxHeaderListSize,
      maxPushes,
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
                               idleTimeout * 1000000,  // millis to nano
                               IDX_QUIC_SESSION_IDLE_TIMEOUT) |
                setConfigField(sessionConfig,
                               maxAckDelay,
                               IDX_QUIC_SESSION_MAX_ACK_DELAY) |
                setConfigField(sessionConfig,
                               maxPacketSize,
                               IDX_QUIC_SESSION_MAX_PACKET_SIZE);

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
                                 IDX_HTTP3_MAX_PUSHES);

  http3Config[IDX_HTTP3_CONFIG_COUNT] = h3flags;
}

// Some events that are emitted originate from the C++ internals and are
// fairly expensive and optional. An aliased array buffer is used to
// communicate that a handler has been added for the optional events
// so that the C++ internals know there is an actual listener. The event
// will not be emitted if there is no handler.
function toggleListeners(self, event, on = 1) {
  if (self[kHandle] === undefined || self.listenerCount(event) !== 0)
    return;
  switch (event) {
    case 'keylog':
      self[kHandle].state[IDX_QUIC_SESSION_STATE_KEYLOG_ENABLED] = on;
      break;
    case 'clientHello':
      self[kHandle].state[IDX_QUIC_SESSION_STATE_CLIENT_HELLO_ENABLED] = on;
      break;
    case 'pathValidation':
      self[kHandle].state[IDX_QUIC_SESSION_STATE_PATH_VALIDATED_ENABLED] = on;
      break;
    case 'OCSPRequest':
      self[kHandle].state[IDX_QUIC_SESSION_STATE_CERT_ENABLED] = on;
      break;
  }
}

module.exports = {
  getAllowUnauthorized,
  getSocketType,
  lookup4,
  lookup6,
  setTransportParams,
  toggleListeners,
  validateCloseCode,
  validateNumberInRange,
  validateTransportParams,
  validateQuicClientSessionOptions,
  validateQuicSocketOptions,
  validateQuicEndpointOptions,
};
