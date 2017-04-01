'use strict';

const binding = process.binding('util');
const prefix = `(${process.release.name}:${process.pid}) `;

const kArrowMessagePrivateSymbolIndex = binding['arrow_message_private_symbol'];
const kDecoratedPrivateSymbolIndex = binding['decorated_private_symbol'];

// The `buffer` module uses this. Defining it here instead of in the public
// `util` module makes it accessible without having to `require('util')` there.
exports.customInspectSymbol = Symbol('util.inspect.custom');

exports.trace = function(msg) {
  console.trace(`${prefix}${msg}`);
};

// Mark that a method should not be used.
// Returns a modified function which warns once by default.
// If --no-deprecation is set, then it is a no-op.
exports.deprecate = function deprecate(fn, msg, code) {
  // Allow for deprecating things in the process of starting up.
  if (global.process === undefined) {
    return function() {
      return exports.deprecate(fn, msg, code).apply(this, arguments);
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
      binding.getHiddenValue(err, kDecoratedPrivateSymbolIndex) === true)
    return;

  const arrow = binding.getHiddenValue(err, kArrowMessagePrivateSymbolIndex);

  if (arrow) {
    err.stack = arrow + err.stack;
    binding.setHiddenValue(err, kDecoratedPrivateSymbolIndex, true);
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
