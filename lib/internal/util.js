'use strict';

const binding = process.binding('util');
const prefix = `(${process.release.name}:${process.pid}) `;

const kArrowMessagePrivateSymbolIndex = binding['arrow_message_private_symbol'];
const kDecoratedPrivateSymbolIndex = binding['decorated_private_symbol'];

exports.getHiddenValue = binding.getHiddenValue;
exports.setHiddenValue = binding.setHiddenValue;

// The `buffer` module uses this. Defining it here instead of in the public
// `util` module makes it accessible without having to `require('util')` there.
exports.customInspectSymbol = Symbol('util.inspect.custom');

// All the internal deprecations have to use this function only, as this will
// prepend the prefix to the actual message.
exports.deprecate = function(fn, msg) {
  return exports._deprecate(fn, msg);
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
    if (!warned) {
      warned = true;
      process.emitWarning(msg, 'DeprecationWarning', deprecated);
    }
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
      exports.getHiddenValue(err, kDecoratedPrivateSymbolIndex) === true)
    return;

  const arrow = exports.getHiddenValue(err, kArrowMessagePrivateSymbolIndex);

  if (arrow) {
    err.stack = arrow + err.stack;
    exports.setHiddenValue(err, kDecoratedPrivateSymbolIndex, true);
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

exports.kIsEncodingSymbol = Symbol('node.isEncoding');
exports.normalizeEncoding = function normalizeEncoding(enc) {
  if (!enc) return 'utf8';
  var low;
  for (;;) {
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
        if (low) return; // undefined
        enc = ('' + enc).toLowerCase();
        low = true;
    }
  }
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
