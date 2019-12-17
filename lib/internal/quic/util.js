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

const {
  constants: {
    AF_INET,
    AF_INET6,
    DEFAULT_RETRYTOKEN_EXPIRATION,
    DEFAULT_MAX_CONNECTIONS_PER_HOST,
    MAX_RETRYTOKEN_EXPIRATION,
    MIN_RETRYTOKEN_EXPIRATION,
    MINIMUM_MAX_CRYPTO_BUFFER,
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
  let closeCode;
  let closeFamily;
  if (code != null && typeof code === 'object') {
    closeCode = code.code || NGTCP2_NO_ERROR;
    closeFamily = code.family || QUIC_ERROR_APPLICATION;
  } else if (typeof code === 'number') {
    closeCode = code;
    closeFamily = QUIC_ERROR_APPLICATION;
  } else {
    throw new ERR_INVALID_ARG_TYPE('code', ['number', 'Object'], code);
  }
  return {
    closeCode,
    closeFamily
  };
}

function validateBindOptions(port, address) {
  if (!isLegalPort(port)) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.port', port, 'is not a valid IP port');
  }
  if (address != null && typeof address !== 'string')
    throw new ERR_INVALID_ARG_TYPE('options.address', 'string', address);
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
    maxCryptoBuffer,
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
  validateNumberInBoundedRange(
    maxCryptoBuffer,
    'options.maxCryptoBuffer',
    MINIMUM_MAX_CRYPTO_BUFFER,
    Number.MAX_SAFE_INTEGER);

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
    maxCryptoBuffer,
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

function validateQuicClientSessionOptions(options) {
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
  } = { ...options };

  if (typeof minDHSize !== 'number')
    throw new ERR_INVALID_ARG_TYPE(
      'options.minDHSize', 'number', minDHSize);

  if (!isLegalPort(port)) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.port', port,
      'is not a valid IP port');
  }

  if (servername && typeof servername !== 'string') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.servername', 'string', servername);
  }
  if (isIP(servername)) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.servername', servername, 'cannot be an IP address');
  }

  if (remoteTransportParams && !isArrayBufferView(remoteTransportParams)) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.remoteTransportParams',
      ['Buffer', 'TypedArray', 'DataView'],
      remoteTransportParams);
  }
  if (sessionTicket && !isArrayBufferView(sessionTicket)) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.sessionTicket',
      ['Buffer', 'TypedArray', 'DataView'],
      sessionTicket);
  }

  if (typeof alpn !== 'string')
    throw new ERR_INVALID_ARG_TYPE('options.alpn', 'string', alpn);

  let dcid;
  if (dcid_value !== undefined) {
    if (typeof dcid_value === 'string') {
      // If it's a string, it must be a hex encoded string
      try {
        dcid = Buffer.from(dcid_value, 'hex');
      } catch {
        throw new ERR_QUICSESSION_INVALID_DCID(dcid);
      }
    } else if (!isArrayBufferView(dcid_value)) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.dcid',
        ['string', 'Buffer', 'TypedArray', 'DataView'],
        dcid);
    } else {
      dcid = dcid_value;
    }
    if (dcid.length > NGTCP2_MAX_CIDLEN ||
        dcid.length < NGTCP2_MIN_CIDLEN) {
      throw new ERR_QUICSESSION_INVALID_DCID(dcid.toString('hex'));
    }
  }

  if (preferredAddressPolicy !== undefined &&
      typeof preferredAddressPolicy !== 'string') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.preferredAddressPolicy',
      'string',
      preferredAddressPolicy);
  }

  if (typeof requestOCSP !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.requestOCSP',
      'boolean',
      requestOCSP);
  }

  if (typeof verifyHostnameIdentity !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.verifyHostnameIdentity',
      'boolean',
      verifyHostnameIdentity);
  }

  if (typeof qlog !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE('options.qlog', 'boolean', qlog);
  }

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

function validateQuicEndpointOptions(options) {
  const {
    address,
    ipv6Only = false,
    lookup,
    port = 0,
    reuseAddr = false,
    type = 'udp4',
    preferred = false,
  } = { ... options };
  validateBindOptions(port, address);
  if (typeof type !== 'string')
    throw new ERR_INVALID_ARG_TYPE('options.type', 'string', type);
  if (lookup !== undefined && typeof lookup !== 'function')
    throw new ERR_INVALID_ARG_TYPE('options.lookup', 'Function', lookup);
  if (typeof ipv6Only !== 'boolean')
    throw new ERR_INVALID_ARG_TYPE('options.ipv6Only', 'boolean', ipv6Only);
  if (typeof reuseAddr !== 'boolean')
    throw new ERR_INVALID_ARG_TYPE('options.reuseAddr', 'boolean', reuseAddr);
  if (typeof preferred !== 'boolean')
    throw new ERR_INVALID_ARG_TYPE('options.preferred', 'boolean', preferred);
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

function validateQuicSocketOptions(options) {
  const {
    endpoint = { port: 0, type: 'udp4' },
    autoClose = false,
    client,
    lookup,
    maxConnectionsPerHost = DEFAULT_MAX_CONNECTIONS_PER_HOST,
    server,
    validateAddress = false,
    validateAddressLRU = false,
    retryTokenTimeout = DEFAULT_RETRYTOKEN_EXPIRATION,
    type = endpoint.type || 'udp4',
    qlog = false,
    statelessResetSecret,
  } = { ...options };
  if (typeof type !== 'string')
    throw new ERR_INVALID_ARG_TYPE('options.type', 'string', type);
  if (endpoint === null || typeof endpoint !== 'object')
    throw new ERR_INVALID_ARG_TYPE('options.endpoint', 'Object', endpoint);
  if (lookup !== undefined && typeof lookup !== 'function')
    throw new ERR_INVALID_ARG_TYPE('options.lookup', 'Function', lookup);
  if (typeof validateAddress !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.validateAddress',
      'boolean',
      validateAddress);
  }
  if (typeof validateAddressLRU !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.validateAddressLRU',
      'boolean',
      validateAddressLRU);
  }
  if (typeof autoClose !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE(
      'options.autoClose',
      'boolean',
      autoClose);
  }
  validateNumberInBoundedRange(
    retryTokenTimeout,
    'options.retryTokenTimeout',
    MIN_RETRYTOKEN_EXPIRATION,
    MAX_RETRYTOKEN_EXPIRATION);
  validateNumberInBoundedRange(
    maxConnectionsPerHost,
    'options.maxConnectionsPerHost',
    1, Number.MAX_SAFE_INTEGER);

  if (typeof qlog !== 'boolean') {
    throw new ERR_INVALID_ARG_TYPE('options.qlog', 'boolean', qlog);
  }

  if (statelessResetSecret !== undefined) {
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


module.exports = {
  getAllowUnauthorized,
  getSocketType,
  lookup4,
  lookup6,
  validateBindOptions,
  validateCloseCode,
  validateNumberInRange,
  validateTransportParams,
  validateQuicClientSessionOptions,
  validateQuicSocketOptions,
  validateQuicEndpointOptions,
};
