'use strict';

// The whole point behind this internal module is to allow Node.js to no
// longer be forced to treat every error message change as a semver-major
// change. The NodeError classes here all expose a `code` property whose
// value identifies the error.

// Note: the lib/_stream_readable.js and lib/_stream_writable.js files
// include their own alternatives to this because those need to be portable
// to the readable-stream standalone module. If the logic here changes at all,
// then those also need to be checked.

const kCode = Symbol('code');

class NodeError extends Error {
  constructor(key /**, ...args **/) {
    const args = Array(arguments.length - 1);
    for (var n = 1; n < arguments.length; n++)
      args[n - 1] = arguments[n];
    super(message(key, args));
    this[kCode] = key;
    Error.captureStackTrace(this, NodeError);
  }

  get name() {
    return `Error[${this[kCode]}]`;
  }

  get code() {
    return this[kCode];
  }
}

class NodeTypeError extends TypeError {
  constructor(key /**, ...args **/) {
    const args = Array(arguments.length - 1);
    for (var n = 1; n < arguments.length; n++)
      args[n - 1] = arguments[n];
    super(message(key, args));
    this[kCode] = key;
    Error.captureStackTrace(this, NodeTypeError);
  }

  get name() {
    return `TypeError[${this[kCode]}]`;
  }

  get code() {
    return this[kCode];
  }
}

class NodeRangeError extends RangeError {
  constructor(key /**, ...args **/) {
    const args = Array(arguments.length - 1);
    for (var n = 1; n < arguments.length; n++)
      args[n - 1] = arguments[n];
    super(message(key, args));
    this[kCode] = key;
    Error.captureStackTrace(this, NodeRangeError);
  }

  get name() {
    return `RangeError[${this[kCode]}]`;
  }

  get code() {
    return this[kCode];
  }
}

var assert, util;
function lazyAssert() {
  if (!assert)
    assert = require('assert');
  return assert;
}

function lazyUtil() {
  if (!util)
    util = require('util');
  return util;
}

function message(key, args) {
  const assert = lazyAssert();
  const util = lazyUtil();
  const msg = exports[Symbol.for(key)];
  assert(msg, `An invalid error message key was used: ${key}.`);
  let fmt = util.format;
  if (typeof msg === 'function') {
    fmt = msg;
  } else {
    if (args === undefined || args.length === 0)
      return msg;
    args.unshift(msg);
  }
  return String(fmt.apply(null, args));
}

exports.message = message;
exports.Error = NodeError;
exports.TypeError = NodeTypeError;
exports.RangeError = NodeRangeError;

// Utility function for registering the error codes.
function E(sym, val) {
  exports[Symbol.for(String(sym).toUpperCase())] =
    typeof val === 'function' ? val : String(val);
}

// To declare an error message, use the E(sym, val) function above. The sym
// must be an upper case string. The val can be either a function or a string.
// The return value of the function must be a string.
// Examples:
// E('EXAMPLE_KEY1', 'This is the error value');
// E('EXAMPLE_KEY2', (a, b) => return `${a} ${b}`);

// Note: Please try to keep these in alphabetical order
E('ASSERTION_ERROR', (msg) => msg);
E('CHANNEL_CLOSED', 'channel closed');
E('CHILD_PROCESS_ASYNC_NO_BUFFER',
  (value) => `Asynchronous forks do not support Buffer input: ${value}`);
E('CHILD_PROCESS_INVALID_STDIO',
  (value) => `Incorrect value of stdio option: ${value}`);
E('CHILD_PROCESS_IPC_DISCONNECTED', 'IPC channel is already disconnected');
E('CHILD_PROCESS_ONE_IPC', 'Child process can have only one IPC pipe');
E('CHILD_PROCESS_SYNCHRONOUS_FORK_NO_IPC',
  'IPC cannot be used with synchronous forks');
E('INVALID_CALLBACK', 'callback is not a function');
E('INVALID_ENCODING', (encoding) => `Unknown encoding: ${encoding}`);
E('INVALID_FILE_OPEN_FLAG', (flag) => `Unknown file open flag: ${flag}`);
E('INVALID_HANDLE_TYPE', 'This handle type cannot be sent');
E('INVALID_PORT', 'port must be a number >= 0 and <= 65535');
E('INVALID_SIGNAL', (signal) => `Unknown signal: ${signal}`);
E('INVALID_URL', 'Invalid URL');
E('NATIVE_MODULE_NOT_FOUND', (module) => `No such native module ${module}`);
E('NO_CRYPTO', 'Node.js is not compiled with openssl crypto support');
E('SOCKET_CLOSED_BEFORE_REPLY', 'Socket closed before reply');
