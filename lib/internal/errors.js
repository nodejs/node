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
  constructor(key, ...args) {
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
  constructor(key, ...args) {
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
  constructor(key, ... args) {
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

exports.Error = NodeError;
exports.TypeError = NodeTypeError;
exports.RangeError = NodeRangeError;

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
  const msg = Error[Symbol.for(key)];
  // We want to assert here because if this happens,
  // Node.js is doing something incorrect.
  assert(msg, `An invalid error message key was used: ${key}.`);
  let fmt = util.format;
  if (typeof msg === 'function') {
    fmt = msg;
  } else {
    args.unshift(msg);
  }
  const res = fmt.apply(null, args);
  // We want to assert here because if this happens,
  // Node.js is doing something incorrect.
  assert(res, 'An invalid formatted error message was returned.');
  return res;
}

// Below is the catalog of messages
// Error keys should be all caps, value can
// be a string formatted for use with
// util.format, or a function.

// Utility function for registering the error codes.
function E(sym, val) {
  Error[Symbol.for(sym)] = val;
}

E('ARGNOTNUM', argNotNum);
E('ASSERTIONERROR', (msg) => msg);
E('BUFFERLIST',
  '"buffer" list argument must contain only Buffers or strings');
E('BUFFERSWAP16', 'Buffer size must be a multiple of 16-bits');
E('BUFFERSWAP32', 'Buffer size must be a multiple of 32-bits');
E('BUFFERTOOLARGE', bufferTooBig);
E('CALLBACKREQUIRED', 'The required "callback" argument is not a function');
E('CHANNELCLOSED', 'Channel closed');
E('CLOSEDBEFOREREPLY', 'Slave closed before reply');
E('CONSOLEWRITABLE', 'Console expects a writable stream instance');
E('DNSUNKNOWNDOMAIN', 'Unable to determine the domain name');
E('ECONNRESET', 'Socket hang up');
E('FILETOOBIG', fileTooBig);
E('HEADERSSENT',
  'Unable to set additional headers after they are already sent');
E('HTTPINVALIDHEADER', 'The header content contains invalid characters');
E('HTTPINVALIDPATH', 'Request path contains unescaped characters');
E('HTTPINVALIDSTATUS', (status) => `Invalid status code: ${status}`);
E('HTTPINVALIDTOKEN', (val) => `Invalid HTTP token: ${val}`);
E('HTTPINVALIDTRAILER', 'The trailer content contains invalid characters');
E('HTTPMETHODINVALID', 'Method must be a valid HTTP token');
E('HTTPUNSUPPORTEDPROTOCOL', unsupportedProtocol);
E('INDEXOUTOFRANGE', 'Index out of range');
E('INVALIDARG', invalidArgument);
E('INVALIDARGVALUE', invalidArgumentValue);
E('INVALIDFAMILY', 'IP protocol family must be either 4 or 6');
E('INVALIDOPT', invalidOption);
E('INVALIDOPTVALUE', invalidOptionValue);
E('INVALIDPID', 'Invalid pid');
E('IPCBADHANDLE', 'Handles of this type cannot be sent');
E('IPCBUFFERASYNCFORK',
  (stdio) => `Asynchronous forks do not support Buffer input: ${stdio}`);
E('IPCDISCONNECTED', 'The IPC channel is already disconnected');
E('IPCNOSYNCFORK', 'IPC cannot be used with synchronous forks');
E('IPCONEIPC', 'Child process can have only one IPC pipe');
E('NOCPUUSAGE', (err) => `Unable to obtain CPU Usage: ${err}`);
E('NOCRYPTO', 'Node.js is not compiled with openssl crypto support');
E('NOTIMPLEMENTED', notImplemented);
E('OUTOFBOUNDSARG', (arg) => `"${arg}" argument is out of bounds`);
E('PORTRANGE', '"port" argument must be a number >= 0 and < 65536');
E('PROTOTYPEREQUIRED',
  'The super constructor to "inherits" must have a prototype');
E('REPLHISTORYPARSE', (path) => `Could not parse history data in ${path}`);
E('REQUIREDARG', requiredArgument);
E('STDERRCLOSE', 'process.stderr cannot be closed');
E('STDOUTCLOSE', 'process.stdout cannot be closed');
E('UNKNOWNENCODING', (enc) => `Unknown encoding: ${enc}`);
E('UNKNOWNSIGNAL', (sig) => `Unknown signal: ${sig}`);
E('UNKNOWNSTATE', 'Unknown state');
E('WRITEOUTOFBOUNDS', 'Write out of bounds');

// Utility methods for the messages above

function bufferTooBig(max) {
  const assert = lazyAssert();
  assert(max !== undefined);
  return 'Cannot create final Buffer. It would be larger ' +
         `than 0x${max.toString(16)} bytes`;
}

function fileTooBig(arg) {
  const assert = lazyAssert();
  assert(arg !== undefined);
  return 'File size is greater than possible Buffer: ' +
         `0x${arg.toString(16)} bytes`;
}

function unsupportedProtocol(protocol, expected) {
  let msg = `Protocol "${protocol}" is not supported`;
  if (expected) {
    msg += `. Expected "${expected}"`;
  }
  return msg;
}

function notImplemented(additional) {
  let msg = 'Not implemented';
  if (additional)
    msg += `: ${additional}`;
  return msg;
}

function invalidOptionValue(opt, val) {
  const assert = lazyAssert();
  assert(opt);
  let msg = `Invalid value for "${opt}" option`;
  if (val)
    msg += `: ${val}`;
  return msg;
}

function invalidArgumentValue(arg, val) {
  const assert = lazyAssert();
  assert(arg);
  let msg = `Invalid value for "${arg}" argument`;
  if (val)
    msg += `: ${val}`;
  return msg;
}

function argNotNum(arg) {
  const assert = lazyAssert();
  assert(arg);
  return `"${arg}" argument must not be a number`;
}

function requiredArgument(arg) {
  const assert = lazyAssert();
  assert(arg);
  return `"${arg}" argument is required and cannot be undefined`;
}

function invalidArgument(arg, expected) {
  return invalidOptionOrArgument(true, arg, expected);
}

function invalidOption(opt, expected) {
  return invalidOptionOrArgument(false, opt, expected);
}

function formatArg(arg) {
  if (typeof arg === 'number') {
    arg >>>= 0;
    switch (arg) {
      case 1:
        arg = 'The first';
        break;
      case 2:
        arg = 'The second';
        break;
      case 3:
        arg = 'The third';
        break;
      default:
        arg = `The ${arg}th`;
    }
  } else {
    arg = `"${arg}"`;
  }
  return arg;
}

// This takes an arg name and one or more expected types to generate an
// appropriate error message indicating that the given argument must be
// of the expected type. For instance:
//  "foo" argument must be a string
//  "bar" argument must be an object
//  "baz" argument must be one of: string, object
//  first argument must be a string
//  ... and so forth.
function invalidOptionOrArgument(isargument, arg, expected) {
  const assert = lazyAssert();
  const util = lazyUtil();
  assert(arg && expected);
  // Is this error message for an argument or an option?
  const what = isargument ? 'argument' : 'option';
  if (typeof expected === 'string') {
    // Check to see if we need "a" or "an".
    const ch = expected.codePointAt(0) | 0x20;
    const prefix = (ch === 0x61 || ch === 0x65 ||
                    ch === 0x69 || ch === 0x6f ||
                    ch === 0x75) ? 'an' : 'a';
    return `${formatArg(arg)} ${what} must be ${prefix} ${expected}`;
  } else if (Array.isArray(expected)) {
    // Is there only a single item in the array or multiple?
    return expected.length === 1 ?
      invalidArgument(arg, expected[0]) :
      `${formatArg(arg)} ${what} must be one of: ${expected.join(', ')}`;
  } else {
    assert.fail(null, null, `Unexpected value: ${util.inspect(expected)}`);
  }
}
