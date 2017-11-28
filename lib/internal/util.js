'use strict';

const binding = process.binding('util');
const prefix = `(${process.release.name}:${process.pid}) `;

exports.getHiddenValue = binding.getHiddenValue;
exports.setHiddenValue = binding.setHiddenValue;

exports.getConstructorOf = function getConstructorOf(obj) {
  while (obj) {
    var descriptor = Object.getOwnPropertyDescriptor(obj, 'constructor');
    if (descriptor !== undefined &&
        typeof descriptor.value === 'function' &&
        descriptor.value.name !== '') {
      return descriptor.value;
    }

    obj = Object.getPrototypeOf(obj);
  }

  return null;
};

// The `buffer` module uses this. Defining it here instead of in the public
// `util` module makes it accessible without having to `require('util')` there.
exports.customInspectSymbol = Symbol('util.inspect.custom');

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
    for (var i = 1; i < arguments.length; ++i)
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
    // Setting this (rather than using Object.setPrototype, as above) ensures
    // that calling the unwrapped constructor gives an instanceof the wrapped
    // constructor.
    deprecated.prototype = fn.prototype;
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
exports.assertCrypto = function() {
  if (noCrypto)
    throw new Error('Node.js is not compiled with openssl crypto support');
};

// Filters duplicate strings. Used to support functions in crypto and tls
// modules. Implemented specifically to maintain existing behaviors in each.
exports.filterDuplicateStrings = function filterDuplicateStrings(items, low) {
  const map = new Map();
  for (var i = 0; i < items.length; i++) {
    const item = items[i];
    const key = item.toLowerCase();
    if (low) {
      map.set(key, key);
    } else {
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
    return result.slice();
  };
};

exports.kIsEncodingSymbol = Symbol('node.isEncoding');

// The loop should only run at most twice, retrying with lowercased enc
// if there is no match in the first pass.
// We use a loop instead of branching to retry with a helper
// function in order to avoid the performance hit.
// Return undefined if there is no match.
exports.normalizeEncoding = function normalizeEncoding(enc) {
  if (!enc) return 'utf8';
  var retried;
  while (true) {
    switch (enc) {
      case 'utf8':
      case 'utf-8':
        return 'utf8';
      case 'ucs2':
      case 'ucs-2':
      case 'utf16le':
      case 'utf-16le':
        return 'utf16le';
      case 'latin1':
      case 'binary':
        return 'latin1';
      case 'base64':
      case 'ascii':
      case 'hex':
        return enc;
      default:
        if (retried) return; // undefined
        enc = ('' + enc).toLowerCase();
        retried = true;
    }
  }
};

/*
 * Implementation of ToInteger as per ECMAScript Specification
 * Refer: http://www.ecma-international.org/ecma-262/6.0/#sec-tointeger
 */
const toInteger = exports.toInteger = function toInteger(argument) {
  const number = +argument;
  return Number.isNaN(number) ? 0 : Math.trunc(number);
};

/*
 * Implementation of ToLength as per ECMAScript Specification
 * Refer: http://www.ecma-international.org/ecma-262/6.0/#sec-tolength
 */
exports.toLength = function toLength(argument) {
  const len = toInteger(argument);
  return len <= 0 ? 0 : Math.min(len, Number.MAX_SAFE_INTEGER);
};

// Cached to make sure no userland code can tamper with it.
const isArrayBufferView = ArrayBuffer.isView;
exports.isArrayBufferView = isArrayBufferView;
