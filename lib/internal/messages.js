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

function replacement(msg, values) {
  if (values.length > 0)
    msg = msg.replace(/{(\d)}/g, (_, index) => values[Number(index)]);
  return msg;
}

function template(message, args, id) {
  return `${replacement(message, args)} (${id})`;
}

function CreateError(ErrorType, key, args) {
  const message = messages.msg(key);
  const err = new ErrorType(template(message.msg, args, message.id));
  err.key = message.id;
  return err;
};

// I18N(key[, ... args])
// Returns: string
function message() {
  const args = [];
  const key = arguments[0];
  for (let n = 1; n < arguments.length; n++)
    args[n - 1] = arguments[n];
  const message = messages.msg(key);
  return replacement(message.msg, args);
}

// I18N.withkey(key[, ... args])
// Returns: string
message.withkey = function() {
  const args = [];
  const key = arguments[0];
  for (let n = 1; n < arguments.length; n++)
    args[n - 1] = arguments[n];
  const message = messages.msg(key);
  return template(message.msg, args, message.id);
};

// I18N.prepare(key)
// Returns: function([...args])
message.prepare = function(key) {
  const message = messages.msg(key);
  return function() {
    const args = [];
    for (let n = 0; n < arguments.length; n++)
      args[n] = arguments[n];
    return replacement(message.msg, args);
  };
};

// I18N.TypeError(key[, ... args])
// Returns: Error
message.Error = function() {
  const args = [];
  const key = arguments[0];
  for (let n = 1; n < arguments.length; n++)
    args[n - 1] = arguments[n];
  return CreateError(Error, key, args);
};

// I18N.TypeError(key[, ... args])
// Returns: TypeError
message.TypeError = function() {
  const args = [];
  const key = arguments[0];
  for (let n = 1; n < arguments.length; n++)
    args[n - 1] = arguments[n];
  return CreateError(TypeError, key, args);
};

// I18N.RangeError(key[, ... args])
// Returns: RangeError
message.RangeError = function() {
  const args = [];
  const key = arguments[0];
  for (let n = 1; n < arguments.length; n++)
    args[n - 1] = arguments[n];
  return CreateError(RangeError, key, args);
};

// I18N.log(key[, ... args])
// Returns: undefined
message.log = function() {
  const args = [];
  const key = arguments[0];
  for (let n = 1; n < arguments.length; n++)
    args[n - 1] = arguments[n];
  const message = messages.msg(key);
  console.log(template(message.msg, args, message.id));
};

// I18N.error(key[, ... args])
// Returns: undefined
message.error = function() {
  const args = [];
  const key = arguments[0];
  for (let n = 1; n < arguments.length; n++)
    args[n - 1] = arguments[n];
  const message = messages.msg(key);
  console.error(template(message.msg, args, message.id));
};

Object.setPrototypeOf(message, messages);

module.exports = message;
