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
 *    throw I18N.TypeError('I18N.ERR_FOO, 'World');
 *    throw I18N.Error(I18N.ERR_FOO, 'World');
 *    throw I18N.RangeError(I18N.ERR_FOO, 'World');
 *
 * These Error objects will include both the expanded message and the
 * string version of the message key in the Error.message, and will
 * include a `.key` property whose value is the string version of the
 * message key.
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

function template(message, args, id) {
  return `${replacement(message, args)} (${id})`;
}

function CreateError(ErrorType, key, args) {
  const _message = msg(key);
  const _id = id(key);
  const err = new ErrorType(template(_message, args, _id));
  err.key = _id;
  return err;
};

// I18N(key[, ... args])
// Returns: string
function message() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  return replacement(msg(key), args);
}

// I18N.withkey(key[, ... args])
// Returns: string
message.withkey = function() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  return template(msg(key), args, id(key));
};

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

// I18N.TypeError(key[, ... args])
// Returns: Error
message.Error = function() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  return CreateError(Error, key, args);
};

// I18N.TypeError(key[, ... args])
// Returns: TypeError
message.TypeError = function() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  return CreateError(TypeError, key, args);
};

// I18N.RangeError(key[, ... args])
// Returns: RangeError
message.RangeError = function() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  return CreateError(RangeError, key, args);
};

// I18N.log(key[, ... args])
// Returns: undefined
message.log = function() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  console.log(template(msg(key), args, id(key)));
};

// I18N.error(key[, ... args])
// Returns: undefined
message.error = function() {
  const key = arguments[0];
  const args = [];
  for (let n = 1; n < arguments.length; n++)
    args.push(arguments[n]);
  console.error(template(msg(key), args, id(key)));
};

const debugs = {};
const debugEnviron = process.env.NODE_DEBUG || '';
message.debuglog = function(set) {
  set = set.toUpperCase();
  if (!debugs[set]) {
    if (new RegExp(`\\b${set}\\b`, 'i').test(debugEnviron)) {
      var pid = process.pid;
      debugs[set] = function(key, ...args) {
        console.error(`${set} ${pid}: ${template(msg(key), args, id(key))}`);
      };
    } else {
      debugs[set] = function() {};
    }
  }
  return debugs[set];
};

Object.setPrototypeOf(message, messages);

module.exports = message;
