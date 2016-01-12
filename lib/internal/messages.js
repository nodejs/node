'use strict';

/**
 * Provides a simple internal API useful for externalizing Node.js' own
 * output. The messages are defined as string templates compiled into
 * the Node binary itself. The format is straightforward:
 *
 * const I18N = require('internal/messages');
 *
 * Message keys are integers defined as constants on the I18N object:
 *   e.g. I18N.ERR_FOO, I18N.ERR_BAR, etc
 *
 * Given a hypothetical key ERR_FOO, with a message "Hello {0}",
 *
 *   const I18N = require('internal/messages');
 *   console.log(I18N(I18N.ERR_FOO, 'World')); // Prints: Hello World
 *
 * If a particular message is expected to be used multiple times, the
 * special `prepare()` method can be used to create a shortcut function:
 *
 *   const I18N = require('internal/messages');
 *   const hello = I18N.prepare(I18N.ERR_FOO);
 *   console.log(hello('World')); // Prints: Hello World
 *
 * As a convenience, it is possible to generate Error objects:
 *
 *    const I18N = require('internal/messages');
 *    throw new I18N.TypeError('I18N.ERR_FOO, 'World');
 *    throw new I18N.Error(I18N.ERR_FOO, 'World');
 *    throw new I18N.RangeError(I18N.ERR_FOO, 'World');
 *
 * These Error objects will include both the expanded message and the
 * string version of the message key in the Error.message. Each will
 * include a `.key` property whose value is the string version of the
 * message key, and a `.args` property whose values is an array of the
 * replacement arguments passed in to generate the message.
 *
 * Calls to console.log() and console.error() can also be replaced with
 * externalized versions:
 *
 *    const I18N = require('internal/messages');
 *    I18N.log(I18N.ERR_FOO, 'World');
 *      // Prints: `Hello World (ERR_FOO)` to stdout
 *    I18N.error(I18N.ERR_FOO, 'World');
 *      // Prints: `Hello World (ERR_FOO)` to stderr
 *
 **/

const messages = process.binding('messages');

function msg(key) {
  return messages.messages[key];
}

function id(key) {
  return messages.keys[key];
}

function replacement(msg, values) {
  if (values && values.length > 0)
    msg = msg.replace(/{(\d)}/g, (_, index) => values[Number(index)]);
  return msg;
}

// I18N(key[, ...args])
// Returns: string
function message() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  return replacement(msg(key), args);
}

// I18N.prepare(key)
// Returns: function([...args])
message.prepare = function(key) {
  const message = msg(key);
  return function() {
    const args = [];
    for (let n = 0; n < arguments.length; n++)
      args.push(arguments[n]);
    return replacement(message, args);
  };
};

class MTypeError extends TypeError {
  constructor() {
    const key = arguments[0];
    const _id = id(key);
    const args = [];
    for (let n = 1; n < arguments.length; n++)
      args.push(arguments[n]);
    super(`${replacement(msg(key), args)} (${_id})`);
    Error.captureStackTrace(this, MTypeError);
    this.key = _id;
    this.args = args;
  }
}
class MRangeError extends RangeError {
  constructor() {
    const key = arguments[0];
    const _id = id(key);
    const args = [];
    for (let n = 1; n < arguments.length; n++)
      args.push(arguments[n]);
    super(`${replacement(msg(key), args)} (${_id})`);
    Error.captureStackTrace(this, MRangeError);
    this.key = _id;
    this.args = args;
  }
}
class MError extends Error {
  constructor() {
    const key = arguments[0];
    const _id = id(key);
    const args = [];
    for (let n = 1; n < arguments.length; n++)
      args.push(arguments[n]);
    super(`${replacement(msg(key), args)} (${_id})`);
    Error.captureStackTrace(this, MError);
    this.key = _id;
    this.args = args;
  }
}

// throw new I18N.TypeError(key[, ...args])
message.TypeError = MTypeError;

// throw new I18N.RangeError(key[, ...args])
message.RangeError = MRangeError;

// throw new I18N.Error(key[, ...args])
message.Error = MError;

// I18N.log(key[, ... args])
// Returns: undefined
message.log = function() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  console.log(`${replacement(msg(key), args)} (${id(key)})`);
};

// I18N.error(key[, ... args])
// Returns: undefined
message.error = function() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  console.error(`${replacement(msg(key), args)} (${id(key)})`);
};

// this is an externalized string aware version of util.debuglog that
// ensures that the string resolution and expansion only happens if
// debug is enabled.
const debugs = {};
const debugEnviron = process.env.NODE_DEBUG || '';
message.debuglog = function(set) {
  set = set.toUpperCase();
  if (!debugs[set]) {
    if (new RegExp(`\\b${set}\\b`, 'i').test(debugEnviron)) {
      var pid = process.pid;
      debugs[set] = function() {
        const key = arguments[0];
        const args = [];
        for (let n = 1; n < arguments.length; n++)
          args.push(arguments[n]);
        console.error(`${set} ${pid}: ${replacement(msg(key), args)}`);
      };
    } else {
      debugs[set] = function() {};
    }
  }
  return debugs[set];
};

Object.setPrototypeOf(message, messages);

module.exports = message;
