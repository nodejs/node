'use strict';

// The whole point behind this internal module is to allow Node.js to no
// longer be forced to treat every error message change as a semver-major
// change. The NodeError classes here all expose a `code` property whose
// value statically and permanently identifies the error. While the error
// message may change, the code should not.

const kCode = Symbol('code');
const messages = new Map();

var util;
function lazyUtil() {
  if (!util)
    util = require('util');
  return util;
}

var assert;
function lazyAssert() {
  if (!assert)
    assert = require('assert');
  return assert;
}

function makeNodeError(Base) {
  return class NodeError extends Base {
    constructor(key, ...args) {
      super(message(key, args));
      this[kCode] = key;
      Error.captureStackTrace(this, NodeError);
    }

    get name() {
      return `${super.name} [${this[kCode]}]`;
    }

    get code() {
      return this[kCode];
    }
  };
}

function message(key, args) {
  const assert = lazyAssert();
  assert.strictEqual(typeof key, 'string');
  const util = lazyUtil();
  const msg = messages.get(key);
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

// Utility function for registering the error codes. Only used here. Exported
// *only* to allow for testing.
function E(sym, val) {
  messages.set(sym, typeof val === 'function' ? val : String(val));
}

module.exports = exports = {
  message,
  Error: makeNodeError(Error),
  TypeError: makeNodeError(TypeError),
  RangeError: makeNodeError(RangeError),
  E // This is exported only to facilitate testing.
};

// To declare an error message, use the E(sym, val) function above. The sym
// must be an upper case string. The val can be either a function or a string.
// The return value of the function must be a string.
// Examples:
// E('EXAMPLE_KEY1', 'This is the error value');
// E('EXAMPLE_KEY2', (a, b) => return `${a} ${b}`);
//
// Once an error code has been assigned, the code itself MUST NOT change and
// any given error code must never be reused to identify a different error.
//
// Any error code added here should also be added to the documentation
//
// Note: Please try to keep these in alphabetical order
E('ERR_ARG_NOT_ITERABLE', '%s must be iterable');
E('ERR_ASSERTION', (msg) => msg);
E('ERR_INVALID_ARG_TYPE', invalidArgType);
E('ERR_INVALID_CALLBACK', 'callback must be a function');
E('ERR_INVALID_FILE_URL_HOST', 'File URL host %s');
E('ERR_INVALID_FILE_URL_PATH', 'File URL path %s');
E('ERR_INVALID_HANDLE_TYPE', 'This handle type cannot be sent');
E('ERR_INVALID_OPT_VALUE',
  (name, value) => {
    return `The value "${String(value)}" is invalid for option "${name}"`;
  });
E('ERR_INVALID_SYNC_FORK_INPUT',
  (value) => {
    return 'Asynchronous forks do not support Buffer, Uint8Array or string' +
           `input: ${value}`;
  });
E('ERR_INVALID_THIS', 'Value of "this" must be of type %s');
E('ERR_INVALID_TUPLE', '%s must be an iterable %s tuple');
E('ERR_INVALID_URL', 'Invalid URL: %s');
E('ERR_INVALID_URL_SCHEME',
  (expected) => `The URL must be ${oneOf(expected, 'scheme')}`);
E('ERR_IPC_CHANNEL_CLOSED', 'channel closed');
E('ERR_IPC_DISCONNECTED', 'IPC channel is already disconnected');
E('ERR_IPC_ONE_PIPE', 'Child process can have only one IPC pipe');
E('ERR_IPC_SYNC_FORK', 'IPC cannot be used with synchronous forks');
E('ERR_MISSING_ARGS', missingArgs);
E('ERR_STDERR_CLOSE', 'process.stderr cannot be closed');
E('ERR_STDOUT_CLOSE', 'process.stdout cannot be closed');
E('ERR_UNKNOWN_BUILTIN_MODULE', (id) => `No such built-in module: ${id}`);
E('ERR_UNKNOWN_SIGNAL', (signal) => `Unknown signal: ${signal}`);
E('ERR_UNKNOWN_STDIN_TYPE', 'Unknown stdin file type');
E('ERR_UNKNOWN_STREAM_TYPE', 'Unknown stream file type');
// Add new errors from here...

function invalidArgType(name, expected, actual) {
  const assert = lazyAssert();
  assert(name, 'name is required');
  var msg = `The "${name}" argument must be ${oneOf(expected, 'type')}`;
  if (arguments.length >= 3) {
    msg += `. Received type ${actual !== null ? typeof actual : 'null'}`;
  }
  return msg;
}

function missingArgs(...args) {
  assert(args.length > 0, 'At least one arg needs to be specified');
  let msg = 'The ';
  const len = args.length;
  args = args.map((a) => `"${a}"`);
  switch (len) {
    case 1:
      msg += `${args[0]} argument`;
      break;
    case 2:
      msg += `${args[0]} and ${args[1]} arguments`;
      break;
    default:
      msg += args.slice(0, len - 1).join(', ');
      msg += `, and ${args[len - 1]} arguments`;
      break;
  }
  return `${msg} must be specified`;
}

function oneOf(expected, thing) {
  assert(expected, 'expected is required');
  assert(typeof thing === 'string', 'thing is required');
  if (Array.isArray(expected)) {
    const len = expected.length;
    assert(len > 0, 'At least one expected value needs to be specified');
    expected = expected.map((i) => String(i));
    if (len > 2) {
      return `one of ${thing} ${expected.slice(0, len - 1).join(', ')}, or ` +
             expected[len - 1];
    } else if (len === 2) {
      return `one of ${thing} ${expected[0]} or ${expected[1]}`;
    } else {
      return `of ${thing} ${expected[0]}`;
    }
  } else {
    return `of ${thing} ${String(expected)}`;
  }
}
