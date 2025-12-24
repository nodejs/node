'use strict';
const { isNativeError } = require('internal/util/types');

/**
 * Serializes an Error object
 * @param {Error} error
 * @returns {object}
 */
function serializeErr(error) {
  if (!isNativeError(error)) {
    return error;
  }

  const obj = {
    __proto__: null,
    type: error.constructor.name,
    message: error.message,
    stack: error.stack,
  };

  // Include additional error properties
  for (const key in error) {
    if (obj[key] === undefined) {
      obj[key] = error[key];
    }
  }

  // Handle error code if present
  if (error.code !== undefined) {
    obj.code = error.code;
  }

  // Handle error cause recursively
  if (error.cause !== undefined) {
    obj.cause = typeof error.cause === 'object' && error.cause !== null ?
      serializeErr(error.cause) :
      error.cause;
  }

  return obj;
}

/**
 * Serializes HTTP request object
 * @param {{method: string, url: string, headers: string, socket: undefined | {remoteAddress: string, remotePort: string}} req - HTTP request
 * @returns {object}
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
 * @param {object} res - HTTP response
 * @returns {object}
 */
function res(res) {
  return {
    statusCode: res.statusCode,
    headers: res.getHeaders ? res.getHeaders() : res.headers,
  };
}

module.exports = {
  err: serializeErr,
  req,
  res,
};
