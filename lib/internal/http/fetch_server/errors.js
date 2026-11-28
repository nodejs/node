'use strict';

// Protocol errors and the error code registry of the Fetch Server API.
// https://fetch-server.proposal.wintertc.org/#errors

const {
  ObjectDefineProperty,
  ReflectGet,
  SafeSet,
  TypeError,
} = primordials;

const {
  lazyDOMException,
} = require('internal/util');

const kRequestRejected = 'ERR_HTTP_REQUEST_REJECTED';
const kRequestCancelled = 'ERR_HTTP_REQUEST_CANCELLED';
const kInternalError = 'ERR_HTTP_INTERNAL_ERROR';
const kStreamReset = 'ERR_HTTP_STREAM_RESET';
const kGoaway = 'ERR_HTTP_GOAWAY';
const kProtocolError = 'ERR_HTTP_PROTOCOL_ERROR';
const kConnectError = 'ERR_HTTP_CONNECT_ERROR';
const kRequestBodyRejected = 'ERR_HTTP_REQUEST_BODY_REJECTED';
const kConnectionReset = 'ERR_HTTP_CONNECTION_RESET';
const kTimeout = 'ERR_HTTP_TIMEOUT';
const kContentTooLarge = 'ERR_HTTP_CONTENT_TOO_LARGE';

// Sets of error codes that may be sent at particular points (Section 15.1).
const kDenySendable = new SafeSet([
  kRequestRejected,
  kRequestCancelled,
  kInternalError,
  kGoaway,
]);
const kDenyConnectSendable = new SafeSet([
  kRequestRejected,
  kRequestCancelled,
  kInternalError,
  kGoaway,
  kConnectError,
]);
const kCommittedSendable = new SafeSet([
  kRequestCancelled,
  kInternalError,
]);
const kTunnelSendable = new SafeSet([
  kRequestCancelled,
  kInternalError,
  kConnectError,
]);

// HTTP/2 error codes (RFC 9113 Section 7).
const kH2Codes = {
  __proto__: null,
  [kRequestRejected]: 0x7,      // REFUSED_STREAM
  [kRequestCancelled]: 0x8,     // CANCEL
  [kInternalError]: 0x2,        // INTERNAL_ERROR
  [kProtocolError]: 0x1,        // PROTOCOL_ERROR
  [kConnectError]: 0xa,         // CONNECT_ERROR
  [kRequestBodyRejected]: 0x0,  // NO_ERROR
};

const kH2Names = [
  'NO_ERROR',
  'PROTOCOL_ERROR',
  'INTERNAL_ERROR',
  'FLOW_CONTROL_ERROR',
  'SETTINGS_TIMEOUT',
  'STREAM_CLOSED',
  'FRAME_SIZE_ERROR',
  'REFUSED_STREAM',
  'CANCEL',
  'COMPRESSION_ERROR',
  'CONNECT_ERROR',
  'ENHANCE_YOUR_CALM',
  'INADEQUATE_SECURITY',
  'HTTP_1_1_REQUIRED',
];

/**
 * Creates a protocol error (Section 15.2): a TypeError with a `code` own property
 * and, optionally, a `detail` own property describing the wire-level signal.
 * @param {string} code
 * @param {string} [message]
 * @param {{ protocol: string, errorCode: number, errorName: string }} [detail]
 * @returns {TypeError}
 */
function createProtocolError(code, message = code, detail) {
  // eslint-disable-next-line no-restricted-syntax
  const err = new TypeError(message);
  ObjectDefineProperty(err, 'code', {
    __proto__: null,
    value: code,
    writable: true,
    enumerable: true,
    configurable: true,
  });
  if (detail !== undefined) {
    ObjectDefineProperty(err, 'detail', {
      __proto__: null,
      value: detail,
      writable: true,
      enumerable: true,
      configurable: true,
    });
  }
  return err;
}

/**
 * Maps an inbound HTTP/2 RST_STREAM code to a protocol error (Section 15.4).
 * @param {number} rstCode
 * @param {string} protocol The ALPN identifier of the connection.
 * @returns {TypeError}
 */
function protocolErrorFromH2(rstCode, protocol) {
  let code;
  switch (rstCode) {
    case 0x1: code = kProtocolError; break;
    case 0x8: code = kRequestCancelled; break;
    case 0xa: code = kConnectError; break;
    default: code = kStreamReset;
  }
  return createProtocolError(code, `Stream reset by peer (${code})`, {
    __proto__: null,
    protocol,
    errorCode: rstCode,
    errorName: kH2Names[rstCode] ?? 'UNKNOWN',
  });
}

/**
 * Gets the error code carried by value, given a set of sendable codes
 * (Section 15.2), or null.
 * @param {any} value
 * @param {SafeSet<string>} sendable
 * @returns {string|null}
 */
function getErrorCode(value, sendable) {
  if (value === null ||
      (typeof value !== 'object' && typeof value !== 'function')) {
    return null;
  }
  let code;
  try {
    code = ReflectGet(value, 'code');
  } catch {
    return null;
  }
  if (typeof code === 'string' && sendable.has(code)) return code;
  return null;
}

function invalidState(message) {
  return lazyDOMException(message, 'InvalidStateError');
}

function notSupported(message) {
  return lazyDOMException(message, 'NotSupportedError');
}

module.exports = {
  createProtocolError,
  getErrorCode,
  invalidState,
  kCommittedSendable,
  kConnectError,
  kConnectionReset,
  kContentTooLarge,
  kDenyConnectSendable,
  kDenySendable,
  kGoaway,
  kH2Codes,
  kInternalError,
  kProtocolError,
  kRequestBodyRejected,
  kRequestCancelled,
  kRequestRejected,
  kStreamReset,
  kTimeout,
  kTunnelSendable,
  notSupported,
  protocolErrorFromH2,
};
