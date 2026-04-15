'use strict';
const {
  ObjectKeys,
  SafeSet,
} = primordials;

const { isNativeError } = require('internal/util/types');

function serializeErr(error) {
  if (!isNativeError(error)) {
    return error;
  }

  const seen = new SafeSet();
  const root = serializeOneError(error, seen);

  let currentErr = error;
  let currentObj = root;
  while ('cause' in currentErr) {
    const cause = currentErr.cause;
    if (typeof cause === 'object' && cause !== null && isNativeError(cause)) {
      if (seen.has(cause)) {
        currentObj.cause = '[Circular]';
        break;
      }
      const serializedCause = serializeOneError(cause, seen);
      currentObj.cause = serializedCause;
      currentErr = cause;
      currentObj = serializedCause;
    } else {
      currentObj.cause = cause;
      break;
    }
  }

  return root;
}

function serializeOneError(error, seen) {
  seen.add(error);

  const obj = {
    __proto__: null,
    type: error.constructor.name,
    message: error.message,
    stack: error.stack,
  };

  const keys = ObjectKeys(error);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    if (key !== 'cause' && obj[key] === undefined) {
      obj[key] = error[key];
    }
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
    __proto__: null,
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
