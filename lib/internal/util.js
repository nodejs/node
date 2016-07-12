'use strict';

const binding = process.binding('util');
const prefix = `(${process.release.name}:${process.pid}) `;

exports.getHiddenValue = binding.getHiddenValue;
exports.setHiddenValue = binding.setHiddenValue;

// All the internal deprecations have to use this function only, as this will
// prepend the prefix to the actual message.
exports.deprecate = function(fn, msg) {
  return exports._deprecate(fn, msg);
};

// All the internal deprecations have to use this function only, as this will
// prepend the prefix to the actual message.
exports.printDeprecationMessage = function(msg, warned, ctor) {
  if (warned || process.noDeprecation)
    return true;
  process.emitWarning(msg, 'DeprecationWarning',
                      ctor || exports.printDeprecationMessage);
  return true;
};

exports.error = function(msg) {
  const fmt = `${prefix}${msg}`;
  if (arguments.length > 1) {
    const args = new Array(arguments.length);
    args[0] = fmt;
    for (let i = 1; i < arguments.length; ++i)
      args[i] = arguments[i];
    console.error.apply(console, args);
  } else {
    console.error(fmt);
  }
};

exports.trace = function(msg) {
  console.trace(`${prefix}${msg}`);
};

// Mark that a method should not be used.
// Returns a modified function which warns once by default.
// If --no-deprecation is set, then it is a no-op.
exports._deprecate = function(fn, msg) {
  // Allow for deprecating things in the process of starting up.
  if (global.process === undefined) {
    return function() {
      return exports._deprecate(fn, msg).apply(this, arguments);
    };
  }

  if (process.noDeprecation === true) {
    return fn;
  }

  var warned = false;
  function deprecated() {
    warned = exports.printDeprecationMessage(msg, warned, deprecated);
    if (new.target) {
      return Reflect.construct(fn, arguments, new.target);
    }
    return fn.apply(this, arguments);
  }

  // The wrapper will keep the same prototype as fn to maintain prototype chain
  Object.setPrototypeOf(deprecated, fn);
  if (fn.prototype) {
    Object.setPrototypeOf(deprecated.prototype, fn.prototype);
  }

  return deprecated;
};

exports.decorateErrorStack = function decorateErrorStack(err) {
  if (!(exports.isError(err) && err.stack) ||
      exports.getHiddenValue(err, 'node:decorated') === true)
    return;

  const arrow = exports.getHiddenValue(err, 'node:arrowMessage');

  if (arrow) {
    err.stack = arrow + err.stack;
    exports.setHiddenValue(err, 'node:decorated', true);
  }
};

exports.isError = function isError(e) {
  return exports.objectToString(e) === '[object Error]' || e instanceof Error;
};

exports.objectToString = function objectToString(o) {
  return Object.prototype.toString.call(o);
};

const noCrypto = !process.versions.openssl;
exports.assertCrypto = function(exports) {
  if (noCrypto)
    throw new Error('Node.js is not compiled with openssl crypto support');
};

// Filters duplicate strings. Used to support functions in crypto and tls
// modules. Implemented specifically to maintain existing behaviors in each.
exports.filterDuplicateStrings = function filterDuplicateStrings(items, low) {
  if (!Array.isArray(items))
    return [];
  const len = items.length;
  if (len <= 1)
    return items;
  const map = new Map();
  for (var i = 0; i < len; i++) {
    const item = items[i];
    const key = item.toLowerCase();
    if (low) {
      map.set(key, key);
    } else {
      if (!map.has(key) || map.get(key) <= item)
        map.set(key, item);
    }
  }
  return Array.from(map.values()).sort();
};

exports.cachedResult = function cachedResult(fn) {
  var result;
  return () => {
    if (result === undefined)
      result = fn();
    return result;
  };
};
