// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const errors = require('internal/errors');
const { inspect } = require('internal/util/inspect');
const {
  ERR_FALSY_VALUE_REJECTION,
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE
} = errors.codes;
const { validateNumber } = require('internal/validators');
const { TextDecoder, TextEncoder } = require('internal/encoding');
const { isBuffer } = require('buffer').Buffer;

const types = internalBinding('types');
Object.assign(types, require('internal/util/types'));
const {
  isRegExp,
  isDate,
} = types;

const {
  deprecate,
  getSystemErrorName: internalErrorName,
  isError,
  promisify,
} = require('internal/util');

let CIRCULAR_ERROR_MESSAGE;
let internalDeepEqual;

function tryStringify(arg) {
  try {
    return JSON.stringify(arg);
  } catch (err) {
    // Populate the circular error message lazily
    if (!CIRCULAR_ERROR_MESSAGE) {
      try {
        const a = {}; a.a = a; JSON.stringify(a);
      } catch (err) {
        CIRCULAR_ERROR_MESSAGE = err.message;
      }
    }
    if (err.name === 'TypeError' && err.message === CIRCULAR_ERROR_MESSAGE)
      return '[Circular]';
    throw err;
  }
}

const emptyOptions = {};
function format(...args) {
  return formatWithOptions(emptyOptions, ...args);
}

function formatWithOptions(inspectOptions, f) {
  let i, tempStr;
  if (typeof f !== 'string') {
    if (arguments.length === 1) return '';
    let res = '';
    for (i = 1; i < arguments.length - 1; i++) {
      res += inspect(arguments[i], inspectOptions);
      res += ' ';
    }
    res += inspect(arguments[i], inspectOptions);
    return res;
  }

  if (arguments.length === 2) return f;

  let str = '';
  let a = 2;
  let lastPos = 0;
  for (i = 0; i < f.length - 1; i++) {
    if (f.charCodeAt(i) === 37) { // '%'
      const nextChar = f.charCodeAt(++i);
      if (a !== arguments.length) {
        switch (nextChar) {
          case 115: // 's'
            tempStr = String(arguments[a++]);
            break;
          case 106: // 'j'
            tempStr = tryStringify(arguments[a++]);
            break;
          case 100: // 'd'
            const tempNum = arguments[a++];
            // eslint-disable-next-line valid-typeof
            if (typeof tempNum === 'bigint') {
              tempStr = `${tempNum}n`;
            } else {
              tempStr = `${Number(tempNum)}`;
            }
            break;
          case 79: // 'O'
            tempStr = inspect(arguments[a++], inspectOptions);
            break;
          case 111: // 'o'
          {
            const opts = Object.assign({}, inspectOptions, {
              showHidden: true,
              showProxy: true
            });
            tempStr = inspect(arguments[a++], opts);
            break;
          }
          case 105: // 'i'
            const tempInteger = arguments[a++];
            // eslint-disable-next-line valid-typeof
            if (typeof tempInteger === 'bigint') {
              tempStr = `${tempInteger}n`;
            } else {
              tempStr = `${parseInt(tempInteger)}`;
            }
            break;
          case 102: // 'f'
            tempStr = `${parseFloat(arguments[a++])}`;
            break;
          case 37: // '%'
            str += f.slice(lastPos, i);
            lastPos = i + 1;
            continue;
          default: // any other character is not a correct placeholder
            continue;
        }
        if (lastPos !== i - 1)
          str += f.slice(lastPos, i - 1);
        str += tempStr;
        lastPos = i + 1;
      } else if (nextChar === 37) {
        str += f.slice(lastPos, i);
        lastPos = i + 1;
      }
    }
  }
  if (lastPos === 0)
    str = f;
  else if (lastPos < f.length)
    str += f.slice(lastPos);
  while (a < arguments.length) {
    const x = arguments[a++];
    if ((typeof x !== 'object' && typeof x !== 'symbol') || x === null) {
      str += ` ${x}`;
    } else {
      str += ` ${inspect(x, inspectOptions)}`;
    }
  }
  return str;
}

const debugs = {};
let debugEnvRegex = /^$/;
if (process.env.NODE_DEBUG) {
  let debugEnv = process.env.NODE_DEBUG;
  debugEnv = debugEnv.replace(/[|\\{}()[\]^$+?.]/g, '\\$&')
      .replace(/\*/g, '.*')
      .replace(/,/g, '$|^')
      .toUpperCase();
  debugEnvRegex = new RegExp(`^${debugEnv}$`, 'i');
}

// Emits warning when user sets
// NODE_DEBUG=http or NODE_DEBUG=http2.
function emitWarningIfNeeded(set) {
  if ('HTTP' === set || 'HTTP2' === set) {
    process.emitWarning('Setting the NODE_DEBUG environment variable ' +
      'to \'' + set.toLowerCase() + '\' can expose sensitive ' +
      'data (such as passwords, tokens and authentication headers) ' +
      'in the resulting log.');
  }
}

function debuglog(set) {
  set = set.toUpperCase();
  if (!debugs[set]) {
    if (debugEnvRegex.test(set)) {
      const pid = process.pid;
      emitWarningIfNeeded(set);
      debugs[set] = function debug() {
        const msg = exports.format.apply(exports, arguments);
        console.error('%s %d: %s', set, pid, msg);
      };
    } else {
      debugs[set] = function debug() {};
    }
  }
  return debugs[set];
}

function isBoolean(arg) {
  return typeof arg === 'boolean';
}

function isNull(arg) {
  return arg === null;
}

function isNullOrUndefined(arg) {
  return arg === null || arg === undefined;
}

function isNumber(arg) {
  return typeof arg === 'number';
}

function isString(arg) {
  return typeof arg === 'string';
}

function isSymbol(arg) {
  return typeof arg === 'symbol';
}

function isUndefined(arg) {
  return arg === undefined;
}

function isObject(arg) {
  return arg !== null && typeof arg === 'object';
}

function isFunction(arg) {
  return typeof arg === 'function';
}

function isPrimitive(arg) {
  return arg === null ||
         typeof arg !== 'object' && typeof arg !== 'function';
}

function pad(n) {
  return n.toString().padStart(2, '0');
}

const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep',
                'Oct', 'Nov', 'Dec'];

// 26 Feb 16:19:34
function timestamp() {
  const d = new Date();
  const time = [pad(d.getHours()),
                pad(d.getMinutes()),
                pad(d.getSeconds())].join(':');
  return [d.getDate(), months[d.getMonth()], time].join(' ');
}

// log is just a thin wrapper to console.log that prepends a timestamp
function log() {
  console.log('%s - %s', timestamp(), exports.format.apply(exports, arguments));
}

/**
 * Inherit the prototype methods from one constructor into another.
 *
 * The Function.prototype.inherits from lang.js rewritten as a standalone
 * function (not on Function.prototype). NOTE: If this file is to be loaded
 * during bootstrapping this function needs to be rewritten using some native
 * functions as prototype setup using normal JavaScript does not work as
 * expected during bootstrapping (see mirror.js in r114903).
 *
 * @param {function} ctor Constructor function which needs to inherit the
 *     prototype.
 * @param {function} superCtor Constructor function to inherit prototype from.
 * @throws {TypeError} Will error if either constructor is null, or if
 *     the super constructor lacks a prototype.
 */
function inherits(ctor, superCtor) {

  if (ctor === undefined || ctor === null)
    throw new ERR_INVALID_ARG_TYPE('ctor', 'Function', ctor);

  if (superCtor === undefined || superCtor === null)
    throw new ERR_INVALID_ARG_TYPE('superCtor', 'Function', superCtor);

  if (superCtor.prototype === undefined) {
    throw new ERR_INVALID_ARG_TYPE('superCtor.prototype',
                                   'Function', superCtor.prototype);
  }
  Object.defineProperty(ctor, 'super_', {
    value: superCtor,
    writable: true,
    configurable: true
  });
  Object.setPrototypeOf(ctor.prototype, superCtor.prototype);
}

function _extend(target, source) {
  // Don't do anything if source isn't an object
  if (source === null || typeof source !== 'object') return target;

  const keys = Object.keys(source);
  let i = keys.length;
  while (i--) {
    target[keys[i]] = source[keys[i]];
  }
  return target;
}

// Deprecated old stuff.

function print(...args) {
  for (var i = 0, len = args.length; i < len; ++i) {
    process.stdout.write(String(args[i]));
  }
}

function puts(...args) {
  for (var i = 0, len = args.length; i < len; ++i) {
    process.stdout.write(`${args[i]}\n`);
  }
}

function debug(x) {
  process.stderr.write(`DEBUG: ${x}\n`);
}

function error(...args) {
  for (var i = 0, len = args.length; i < len; ++i) {
    process.stderr.write(`${args[i]}\n`);
  }
}

function callbackifyOnRejected(reason, cb) {
  // `!reason` guard inspired by bluebird (Ref: https://goo.gl/t5IS6M).
  // Because `null` is a special error value in callbacks which means "no error
  // occurred", we error-wrap so the callback consumer can distinguish between
  // "the promise rejected with null" or "the promise fulfilled with undefined".
  if (!reason) {
    const newReason = new ERR_FALSY_VALUE_REJECTION();
    newReason.reason = reason;
    reason = newReason;
    Error.captureStackTrace(reason, callbackifyOnRejected);
  }
  return cb(reason);
}

function callbackify(original) {
  if (typeof original !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('original', 'Function', original);
  }

  // We DO NOT return the promise as it gives the user a false sense that
  // the promise is actually somehow related to the callback's execution
  // and that the callback throwing will reject the promise.
  function callbackified(...args) {
    const maybeCb = args.pop();
    if (typeof maybeCb !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('last argument', 'Function', maybeCb);
    }
    const cb = (...args) => { Reflect.apply(maybeCb, this, args); };
    // In true node style we process the callback on `nextTick` with all the
    // implications (stack, `uncaughtException`, `async_hooks`)
    Reflect.apply(original, this, args)
      .then((ret) => process.nextTick(cb, null, ret),
            (rej) => process.nextTick(callbackifyOnRejected, rej, cb));
  }

  Object.setPrototypeOf(callbackified, Object.getPrototypeOf(original));
  Object.defineProperties(callbackified,
                          Object.getOwnPropertyDescriptors(original));
  return callbackified;
}

function getSystemErrorName(err) {
  validateNumber(err, 'err');
  if (err >= 0 || !Number.isSafeInteger(err)) {
    throw new ERR_OUT_OF_RANGE('err', 'a negative integer', err);
  }
  return internalErrorName(err);
}

// Keep the `exports =` so that various functions can still be monkeypatched
module.exports = exports = {
  _errnoException: errors.errnoException,
  _exceptionWithHostPort: errors.exceptionWithHostPort,
  _extend,
  callbackify,
  debuglog,
  deprecate,
  format,
  formatWithOptions,
  getSystemErrorName,
  inherits,
  inspect,
  isArray: Array.isArray,
  isBoolean,
  isBuffer,
  isDeepStrictEqual(a, b) {
    if (internalDeepEqual === undefined) {
      internalDeepEqual = require('internal/util/comparisons')
        .isDeepStrictEqual;
    }
    return internalDeepEqual(a, b);
  },
  isNull,
  isNullOrUndefined,
  isNumber,
  isString,
  isSymbol,
  isUndefined,
  isRegExp,
  isObject,
  isDate,
  isError,
  isFunction,
  isPrimitive,
  log,
  promisify,
  TextDecoder,
  TextEncoder,
  types,

  // Deprecated Old Stuff
  debug: deprecate(debug,
                   'util.debug is deprecated. Use console.error instead.',
                   'DEP0028'),
  error: deprecate(error,
                   'util.error is deprecated. Use console.error instead.',
                   'DEP0029'),
  print: deprecate(print,
                   'util.print is deprecated. Use console.log instead.',
                   'DEP0026'),
  puts: deprecate(puts,
                  'util.puts is deprecated. Use console.log instead.',
                  'DEP0027')
};
