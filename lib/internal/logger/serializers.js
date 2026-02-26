'use strict';
const {
  ObjectKeys,
  SafeSet,
} = primordials;

const { isNativeError } = require('internal/util/types');

/**
 * Serializes an Error object with circular reference protection
 * @param {Error} error
 * @param {Set} [seen] - Set to track visited errors for circular detection
 * @returns {{ type: string, message: string, stack: string, code?: any, cause?: any } | *}
 */
function serializeErr(error, seen) {
  // Pass through non-Error values (e.g. logger.error({ err: someValue }))
  if (!isNativeError(error)) {
    return error;
  }

  // Circular reference protection
  if (seen === undefined) {
    seen = new SafeSet();
  }
  if (seen.has(error)) {
    return '[Circular]';
  }
  seen.add(error);

  const obj = {
    __proto__: null,
    type: error.constructor.name,
    message: error.message,
    stack: error.stack,
  };

  // Include additional own error properties (avoid prototype chain)
  const keys = ObjectKeys(error);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    if (obj[key] === undefined) {
      obj[key] = error[key];
    }
  }

  // Handle error cause recursively
  if (error.cause !== undefined) {
    obj.cause = typeof error.cause === 'object' && error.cause !== null ?
      serializeErr(error.cause, seen) :
      error.cause;
  }

  return obj;
}

/**
 * Serializes HTTP request object
 * @param {{ method: string, url: string, headers: object, socket?: {
 *  remoteAddress?: string, remotePort?: string
 * } }} req - HTTP request
 * @returns {{ method: string, url: string, headers: object, remoteAddress?: string, remotePort?: string }}
 */
function req(req) {
  return {
    method: req.method,
    url: req.url,
    headers: req.headers,
    remoteAddress: req.socket?.remoteAddress,
    remotePort: req.socket?.remotePort,
  };
}

/**
 * Serializes HTTP response object
 * @param {{ statusCode: number, getHeaders?: () => object, headers?: object }} res - HTTP response
 * @returns {{ statusCode: number, headers: object }}
 */
function res(res) {
  return {
    statusCode: res.statusCode,
    headers: res.getHeaders?.() ?? res.headers,
  };
}

module.exports = {
  err: serializeErr,
  req,
  res,
};
