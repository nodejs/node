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
const { TextDecoder, TextEncoder } = require('internal/encoding');
const { isBuffer } = require('buffer').Buffer;

const { errname } = process.binding('uv');

const {
  getPromiseDetails,
  getProxyDetails,
  isAnyArrayBuffer,
  isDataView,
  isExternal,
  isMap,
  isMapIterator,
  isPromise,
  isSet,
  isSetIterator,
  isRegExp,
  isDate,
  kPending,
  kRejected,
} = process.binding('util');

const {
  isTypedArray
} = require('internal/util/types');

const {
  customInspectSymbol,
  deprecate,
  getConstructorOf,
  isError,
  promisify,
  join
} = require('internal/util');

const inspectDefaultOptions = Object.seal({
  showHidden: false,
  depth: 2,
  colors: false,
  customInspect: true,
  showProxy: false,
  maxArrayLength: 100,
  breakLength: 60
});

const propertyIsEnumerable = Object.prototype.propertyIsEnumerable;
const regExpToString = RegExp.prototype.toString;
const dateToISOString = Date.prototype.toISOString;
const errorToString = Error.prototype.toString;

var CIRCULAR_ERROR_MESSAGE;
var Debug;

/* eslint-disable */
const strEscapeSequencesRegExp = /[\x00-\x1f\x27\x5c]/;
const keyEscapeSequencesRegExp = /[\x00-\x1f\x27]/;
const strEscapeSequencesReplacer = /[\x00-\x1f\x27\x5c]/g;
const keyEscapeSequencesReplacer = /[\x00-\x1f\x27]/g;
/* eslint-enable */
const keyStrRegExp = /^[a-zA-Z_][a-zA-Z_0-9]*$/;
const colorRegExp = /\u001b\[\d\d?m/g;
const numberRegExp = /^(0|[1-9][0-9]*)$/;

// Escaped special characters. Use empty strings to fill up unused entries.
const meta = [
  '\\u0000', '\\u0001', '\\u0002', '\\u0003', '\\u0004',
  '\\u0005', '\\u0006', '\\u0007', '\\b', '\\t',
  '\\n', '\\u000b', '\\f', '\\r', '\\u000e',
  '\\u000f', '\\u0010', '\\u0011', '\\u0012', '\\u0013',
  '\\u0014', '\\u0015', '\\u0016', '\\u0017', '\\u0018',
  '\\u0019', '\\u001a', '\\u001b', '\\u001c', '\\u001d',
  '\\u001e', '\\u001f', '', '', '',
  '', '', '', '', "\\'", '', '', '', '', '',
  '', '', '', '', '', '', '', '', '', '',
  '', '', '', '', '', '', '', '', '', '',
  '', '', '', '', '', '', '', '', '', '',
  '', '', '', '', '', '', '', '', '', '',
  '', '', '', '', '', '', '', '\\\\'
];

const escapeFn = (str) => meta[str.charCodeAt(0)];

// Escape control characters, single quotes and the backslash.
// This is similar to JSON stringify escaping.
function strEscape(str) {
  // Some magic numbers that worked out fine while benchmarking with v8 6.0
  if (str.length < 5000 && !strEscapeSequencesRegExp.test(str))
    return `'${str}'`;
  if (str.length > 100)
    return `'${str.replace(strEscapeSequencesReplacer, escapeFn)}'`;
  var result = '';
  var last = 0;
  for (var i = 0; i < str.length; i++) {
    const point = str.charCodeAt(i);
    if (point === 39 || point === 92 || point < 32) {
      if (last === i) {
        result += meta[point];
      } else {
        result += `${str.slice(last, i)}${meta[point]}`;
      }
      last = i + 1;
    }
  }
  if (last === 0) {
    result = str;
  } else if (last !== i) {
    result += str.slice(last);
  }
  return `'${result}'`;
}

// Escape control characters and single quotes.
// Note: for performance reasons this is not combined with strEscape
function keyEscape(str) {
  if (str.length < 5000 && !keyEscapeSequencesRegExp.test(str))
    return `'${str}'`;
  if (str.length > 100)
    return `'${str.replace(keyEscapeSequencesReplacer, escapeFn)}'`;
  var result = '';
  var last = 0;
  for (var i = 0; i < str.length; i++) {
    const point = str.charCodeAt(i);
    if (point === 39 || point < 32) {
      if (last === i) {
        result += meta[point];
      } else {
        result += `${str.slice(last, i)}${meta[point]}`;
      }
      last = i + 1;
    }
  }
  if (last === 0) {
    result = str;
  } else if (last !== i) {
    result += str.slice(last);
  }
  return `'${result}'`;
}

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

function format(f) {
  var i, tempStr;
  if (typeof f !== 'string') {
    if (arguments.length === 0) return '';
    var res = '';
    for (i = 0; i < arguments.length - 1; i++) {
      res += inspect(arguments[i]);
      res += ' ';
    }
    res += inspect(arguments[i]);
    return res;
  }

  if (arguments.length === 1) return f;

  var str = '';
  var a = 1;
  var lastPos = 0;
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
            tempStr = `${Number(arguments[a++])}`;
            break;
          case 79: // 'O'
            tempStr = inspect(arguments[a++]);
            break;
          case 111: // 'o'
            tempStr = inspect(arguments[a++],
                              { showHidden: true, depth: 4, showProxy: true });
            break;
          case 105: // 'i'
            tempStr = `${parseInt(arguments[a++])}`;
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
      str += ` ${inspect(x)}`;
    }
  }
  return str;
}

var debugs = {};
var debugEnviron;

function debuglog(set) {
  if (debugEnviron === undefined) {
    debugEnviron = new Set(
      (process.env.NODE_DEBUG || '').split(',').map((s) => s.toUpperCase()));
  }
  set = set.toUpperCase();
  if (!debugs[set]) {
    if (debugEnviron.has(set)) {
      var pid = process.pid;
      debugs[set] = function() {
        var msg = exports.format.apply(exports, arguments);
        console.error('%s %d: %s', set, pid, msg);
      };
    } else {
      debugs[set] = function() {};
    }
  }
  return debugs[set];
}

/**
 * Echos the value of a value. Tries to print the value out
 * in the best way possible given the different types.
 *
 * @param {Object} obj The object to print out.
 * @param {Object} opts Optional options object that alters the output.
 */
/* Legacy: obj, showHidden, depth, colors*/
function inspect(obj, opts) {
  // Default options
  const ctx = {
    seen: [],
    stylize: stylizeNoColor,
    showHidden: inspectDefaultOptions.showHidden,
    depth: inspectDefaultOptions.depth,
    colors: inspectDefaultOptions.colors,
    customInspect: inspectDefaultOptions.customInspect,
    showProxy: inspectDefaultOptions.showProxy,
    maxArrayLength: inspectDefaultOptions.maxArrayLength,
    breakLength: inspectDefaultOptions.breakLength,
    indentationLvl: 0
  };
  // Legacy...
  if (arguments.length > 2) {
    if (arguments[2] !== undefined) {
      ctx.depth = arguments[2];
    }
    if (arguments.length > 3 && arguments[3] !== undefined) {
      ctx.colors = arguments[3];
    }
  }
  // Set user-specified options
  if (typeof opts === 'boolean') {
    ctx.showHidden = opts;
  } else if (opts) {
    const optKeys = Object.keys(opts);
    for (var i = 0; i < optKeys.length; i++) {
      ctx[optKeys[i]] = opts[optKeys[i]];
    }
  }
  if (ctx.colors) ctx.stylize = stylizeWithColor;
  if (ctx.maxArrayLength === null) ctx.maxArrayLength = Infinity;
  return formatValue(ctx, obj, ctx.depth);
}
inspect.custom = customInspectSymbol;

Object.defineProperty(inspect, 'defaultOptions', {
  get() {
    return inspectDefaultOptions;
  },
  set(options) {
    if (options === null || typeof options !== 'object') {
      throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'options', 'object');
    }
    Object.assign(inspectDefaultOptions, options);
    return inspectDefaultOptions;
  }
});

// http://en.wikipedia.org/wiki/ANSI_escape_code#graphics
inspect.colors = Object.assign(Object.create(null), {
  'bold': [1, 22],
  'italic': [3, 23],
  'underline': [4, 24],
  'inverse': [7, 27],
  'white': [37, 39],
  'grey': [90, 39],
  'black': [30, 39],
  'blue': [34, 39],
  'cyan': [36, 39],
  'green': [32, 39],
  'magenta': [35, 39],
  'red': [31, 39],
  'yellow': [33, 39]
});

// Don't use 'blue' not visible on cmd.exe
inspect.styles = Object.assign(Object.create(null), {
  'special': 'cyan',
  'number': 'yellow',
  'boolean': 'yellow',
  'undefined': 'grey',
  'null': 'bold',
  'string': 'green',
  'symbol': 'green',
  'date': 'magenta',
  // "name": intentionally not styling
  'regexp': 'red'
});

function stylizeWithColor(str, styleType) {
  const style = inspect.styles[styleType];
  if (style !== undefined) {
    const color = inspect.colors[style];
    return `\u001b[${color[0]}m${str}\u001b[${color[1]}m`;
  }
  return str;
}

function stylizeNoColor(str, styleType) {
  return str;
}

function ensureDebugIsInitialized() {
  if (Debug === undefined) {
    const runInDebugContext = require('vm').runInDebugContext;
    Debug = runInDebugContext('Debug');
  }
}

function formatValue(ctx, value, recurseTimes, ln) {
  // Primitive types cannot have properties
  if (typeof value !== 'object' && typeof value !== 'function') {
    return formatPrimitive(ctx.stylize, value);
  }
  if (value === null) {
    return ctx.stylize('null', 'null');
  }

  if (ctx.showProxy) {
    const proxy = getProxyDetails(value);
    if (proxy !== undefined) {
      if (recurseTimes != null) {
        if (recurseTimes < 0)
          return ctx.stylize('Proxy [Array]', 'special');
        recurseTimes -= 1;
      }
      ctx.indentationLvl += 2;
      const res = [
        formatValue(ctx, proxy[0], recurseTimes),
        formatValue(ctx, proxy[1], recurseTimes)
      ];
      ctx.indentationLvl -= 2;
      const str = reduceToSingleString(ctx, res, '', ['[', ']']);
      return `Proxy ${str}`;
    }
  }

  // Provide a hook for user-specified inspect functions.
  // Check that value is an object with an inspect function on it
  if (ctx.customInspect) {
    const maybeCustomInspect = value[customInspectSymbol] || value.inspect;

    if (typeof maybeCustomInspect === 'function' &&
        // Filter out the util module, its inspect function is special
        maybeCustomInspect !== exports.inspect &&
        // Also filter out any prototype objects using the circular check.
        !(value.constructor && value.constructor.prototype === value)) {
      const ret = maybeCustomInspect.call(value, recurseTimes, ctx);

      // If the custom inspection method returned `this`, don't go into
      // infinite recursion.
      if (ret !== value) {
        if (typeof ret !== 'string') {
          return formatValue(ctx, ret, recurseTimes);
        }
        return ret;
      }
    }
  }

  var keys;
  var symbols = Object.getOwnPropertySymbols(value);

  // Look up the keys of the object.
  if (ctx.showHidden) {
    keys = Object.getOwnPropertyNames(value);
  } else {
    keys = Object.keys(value);
    if (symbols.length !== 0)
      symbols = symbols.filter((key) => propertyIsEnumerable.call(value, key));
  }

  const keyLength = keys.length + symbols.length;
  const constructor = getConstructorOf(value);
  const ctorName = constructor && constructor.name ?
    `${constructor.name} ` : '';

  var base = '';
  var formatter = formatObject;
  var braces;
  var noIterator = true;
  var raw;

  // Iterators and the rest are split to reduce checks
  if (value[Symbol.iterator]) {
    noIterator = false;
    if (Array.isArray(value)) {
      // Only set the constructor for non ordinary ("Array [...]") arrays.
      braces = [`${ctorName === 'Array ' ? '' : ctorName}[`, ']'];
      if (value.length === 0 && keyLength === 0)
        return `${braces[0]}]`;
      formatter = formatArray;
    } else if (isSet(value)) {
      if (value.size === 0 && keyLength === 0)
        return `${ctorName}{}`;
      braces = [`${ctorName}{`, '}'];
      formatter = formatSet;
    } else if (isMap(value)) {
      if (value.size === 0 && keyLength === 0)
        return `${ctorName}{}`;
      braces = [`${ctorName}{`, '}'];
      formatter = formatMap;
    } else if (isTypedArray(value)) {
      braces = [`${ctorName}[`, ']'];
      formatter = formatTypedArray;
    } else if (isMapIterator(value)) {
      braces = ['MapIterator {', '}'];
      formatter = formatCollectionIterator;
    } else if (isSetIterator(value)) {
      braces = ['SetIterator {', '}'];
      formatter = formatCollectionIterator;
    } else {
      // Check for boxed strings with valueOf()
      // The .valueOf() call can fail for a multitude of reasons
      try {
        raw = value.valueOf();
      } catch (e) { /* ignore */ }

      if (typeof raw === 'string') {
        const formatted = formatPrimitive(stylizeNoColor, raw);
        if (keyLength === raw.length)
          return ctx.stylize(`[String: ${formatted}]`, 'string');
        base = ` [String: ${formatted}]`;
        // For boxed Strings, we have to remove the 0-n indexed entries,
        // since they just noisy up the output and are redundant
        // Make boxed primitive Strings look like such
        keys = keys.slice(value.length);
        braces = ['{', '}'];
      } else {
        noIterator = true;
      }
    }
  }
  if (noIterator) {
    braces = ['{', '}'];
    if (ctorName === 'Object ') {
      // Object fast path
      if (keyLength === 0)
        return '{}';
    } else if (typeof value === 'function') {
      const name = `${constructor.name}${value.name ? `: ${value.name}` : ''}`;
      if (keyLength === 0)
        return ctx.stylize(`[${name}]`, 'special');
      base = ` [${name}]`;
    } else if (isRegExp(value)) {
      // Make RegExps say that they are RegExps
      if (keyLength === 0 || recurseTimes < 0)
        return ctx.stylize(regExpToString.call(value), 'regexp');
      base = ` ${regExpToString.call(value)}`;
    } else if (isDate(value)) {
      if (keyLength === 0) {
        if (Number.isNaN(value.getTime()))
          return ctx.stylize(value.toString(), 'date');
        return ctx.stylize(dateToISOString.call(value), 'date');
      }
      // Make dates with properties first say the date
      base = ` ${dateToISOString.call(value)}`;
    } else if (isError(value)) {
      // Make error with message first say the error
      if (keyLength === 0)
        return formatError(value);
      base = ` ${formatError(value)}`;
    } else if (isAnyArrayBuffer(value)) {
      // Fast path for ArrayBuffer and SharedArrayBuffer.
      // Can't do the same for DataView because it has a non-primitive
      // .buffer property that we need to recurse for.
      if (keyLength === 0)
        return ctorName +
              `{ byteLength: ${formatNumber(ctx.stylize, value.byteLength)} }`;
      braces[0] = `${ctorName}{`;
      keys.unshift('byteLength');
    } else if (isDataView(value)) {
      braces[0] = `${ctorName}{`;
      // .buffer goes last, it's not a primitive like the others.
      keys.unshift('byteLength', 'byteOffset', 'buffer');
    } else if (isPromise(value)) {
      braces[0] = `${ctorName}{`;
      formatter = formatPromise;
    } else {
      // Check boxed primitives other than string with valueOf()
      // NOTE: `Date` has to be checked first!
      // The .valueOf() call can fail for a multitude of reasons
      try {
        raw = value.valueOf();
      } catch (e) { /* ignore */ }

      if (typeof raw === 'number') {
        // Make boxed primitive Numbers look like such
        const formatted = formatPrimitive(stylizeNoColor, raw);
        if (keyLength === 0)
          return ctx.stylize(`[Number: ${formatted}]`, 'number');
        base = ` [Number: ${formatted}]`;
      } else if (typeof raw === 'boolean') {
        // Make boxed primitive Booleans look like such
        const formatted = formatPrimitive(stylizeNoColor, raw);
        if (keyLength === 0)
          return ctx.stylize(`[Boolean: ${formatted}]`, 'boolean');
        base = ` [Boolean: ${formatted}]`;
      } else if (typeof raw === 'symbol') {
        const formatted = formatPrimitive(stylizeNoColor, raw);
        return ctx.stylize(`[Symbol: ${formatted}]`, 'symbol');
      } else if (keyLength === 0) {
        if (isExternal(value))
          return ctx.stylize('[External]', 'special');
        return `${ctorName}{}`;
      } else {
        braces[0] = `${ctorName}{`;
      }
    }
  }

  // Using an array here is actually better for the average case than using
  // a Set. `seen` will only check for the depth and will never grow to large.
  if (ctx.seen.indexOf(value) !== -1)
    return ctx.stylize('[Circular]', 'special');

  if (recurseTimes != null) {
    if (recurseTimes < 0)
      return ctx.stylize(`[${constructor ? constructor.name : 'Object'}]`,
                         'special');
    recurseTimes -= 1;
  }

  ctx.seen.push(value);
  const output = formatter(ctx, value, recurseTimes, keys);

  for (var i = 0; i < symbols.length; i++) {
    output.push(formatProperty(ctx, value, recurseTimes, symbols[i], 0));
  }
  ctx.seen.pop();

  return reduceToSingleString(ctx, output, base, braces, ln);
}

function formatNumber(fn, value) {
  // Format -0 as '-0'. A `value === -0` check won't distinguish 0 from -0.
  // Using a division check is currently faster than `Object.is(value, -0)`
  // as of V8 6.1.
  if (1 / value === -Infinity)
    return fn('-0', 'number');
  return fn(`${value}`, 'number');
}

function formatPrimitive(fn, value) {
  if (typeof value === 'string')
    return fn(strEscape(value), 'string');
  if (typeof value === 'number')
    return formatNumber(fn, value);
  if (typeof value === 'boolean')
    return fn(`${value}`, 'boolean');
  if (typeof value === 'undefined')
    return fn('undefined', 'undefined');
  // es6 symbol primitive
  return fn(value.toString(), 'symbol');
}

function formatError(value) {
  return value.stack || `[${errorToString.call(value)}]`;
}

function formatObject(ctx, value, recurseTimes, keys) {
  const len = keys.length;
  const output = new Array(len);
  for (var i = 0; i < len; i++)
    output[i] = formatProperty(ctx, value, recurseTimes, keys[i], 0);
  return output;
}

// The array is sparse and/or has extra keys
function formatSpecialArray(ctx, value, recurseTimes, keys, maxLength, valLen) {
  const output = [];
  const keyLen = keys.length;
  var visibleLength = 0;
  var i = 0;
  if (keyLen !== 0 && numberRegExp.test(keys[0])) {
    for (const key of keys) {
      if (visibleLength === maxLength)
        break;
      const index = +key;
      // Arrays can only have up to 2^32 - 1 entries
      if (index > 2 ** 32 - 2)
        break;
      if (i !== index) {
        if (!numberRegExp.test(key))
          break;
        const emptyItems = index - i;
        const ending = emptyItems > 1 ? 's' : '';
        const message = `<${emptyItems} empty item${ending}>`;
        output.push(ctx.stylize(message, 'undefined'));
        i = index;
        if (++visibleLength === maxLength)
          break;
      }
      output.push(formatProperty(ctx, value, recurseTimes, key, 1));
      visibleLength++;
      i++;
    }
  }
  if (i < valLen && visibleLength !== maxLength) {
    const len = valLen - i;
    const ending = len > 1 ? 's' : '';
    const message = `<${len} empty item${ending}>`;
    output.push(ctx.stylize(message, 'undefined'));
    i = valLen;
    if (keyLen === 0)
      return output;
  }
  const remaining = valLen - i;
  if (remaining > 0) {
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  }
  if (ctx.showHidden && keys[keyLen - 1] === 'length') {
    // No extra keys
    output.push(formatProperty(ctx, value, recurseTimes, 'length', 2));
  } else if (valLen === 0 ||
    keyLen > valLen && keys[valLen - 1] === `${valLen - 1}`) {
    // The array is not sparse
    for (i = valLen; i < keyLen; i++)
      output.push(formatProperty(ctx, value, recurseTimes, keys[i], 2));
  } else if (keys[keyLen - 1] !== `${valLen - 1}`) {
    const extra = [];
    // Only handle special keys
    var key;
    for (i = keys.length - 1; i >= 0; i--) {
      key = keys[i];
      if (numberRegExp.test(key) && +key < 2 ** 32 - 1)
        break;
      extra.push(formatProperty(ctx, value, recurseTimes, key, 2));
    }
    for (i = extra.length - 1; i >= 0; i--)
      output.push(extra[i]);
  }
  return output;
}

function formatArray(ctx, value, recurseTimes, keys) {
  const len = Math.min(Math.max(0, ctx.maxArrayLength), value.length);
  const hidden = ctx.showHidden ? 1 : 0;
  const valLen = value.length;
  const keyLen = keys.length - hidden;
  if (keyLen !== valLen || keys[keyLen - 1] !== `${valLen - 1}`)
    return formatSpecialArray(ctx, value, recurseTimes, keys, len, valLen);

  const remaining = valLen - len;
  const output = new Array(len + (remaining > 0 ? 1 : 0) + hidden);
  for (var i = 0; i < len; i++)
    output[i] = formatProperty(ctx, value, recurseTimes, keys[i], 1);
  if (remaining > 0)
    output[i++] = `... ${remaining} more item${remaining > 1 ? 's' : ''}`;
  if (ctx.showHidden === true)
    output[i] = formatProperty(ctx, value, recurseTimes, 'length', 2);
  return output;
}

function formatTypedArray(ctx, value, recurseTimes, keys) {
  const maxLength = Math.min(Math.max(0, ctx.maxArrayLength), value.length);
  const remaining = value.length - maxLength;
  const output = new Array(maxLength + (remaining > 0 ? 1 : 0));
  for (var i = 0; i < maxLength; ++i)
    output[i] = formatNumber(ctx.stylize, value[i]);
  if (remaining > 0)
    output[i] = `... ${remaining} more item${remaining > 1 ? 's' : ''}`;
  if (ctx.showHidden) {
    // .buffer goes last, it's not a primitive like the others.
    const extraKeys = [
      'BYTES_PER_ELEMENT',
      'length',
      'byteLength',
      'byteOffset',
      'buffer'
    ];
    for (i = 0; i < extraKeys.length; i++) {
      const str = formatValue(ctx, value[extraKeys[i]], recurseTimes);
      output.push(`[${extraKeys[i]}]: ${str}`);
    }
  }
  // TypedArrays cannot have holes. Therefore it is safe to assume that all
  // extra keys are indexed after value.length.
  for (i = value.length; i < keys.length; i++) {
    output.push(formatProperty(ctx, value, recurseTimes, keys[i], 2));
  }
  return output;
}

function formatSet(ctx, value, recurseTimes, keys) {
  const output = new Array(value.size + keys.length + (ctx.showHidden ? 1 : 0));
  var i = 0;
  for (const v of value)
    output[i++] = formatValue(ctx, v, recurseTimes);
  // With `showHidden`, `length` will display as a hidden property for
  // arrays. For consistency's sake, do the same for `size`, even though this
  // property isn't selected by Object.getOwnPropertyNames().
  if (ctx.showHidden)
    output[i++] = `[size]: ${ctx.stylize(`${value.size}`, 'number')}`;
  for (var n = 0; n < keys.length; n++) {
    output[i++] = formatProperty(ctx, value, recurseTimes, keys[n], 0);
  }
  return output;
}

function formatMap(ctx, value, recurseTimes, keys) {
  const output = new Array(value.size + keys.length + (ctx.showHidden ? 1 : 0));
  var i = 0;
  for (const [k, v] of value)
    output[i++] = `${formatValue(ctx, k, recurseTimes)} => ` +
      formatValue(ctx, v, recurseTimes);
  // See comment in formatSet
  if (ctx.showHidden)
    output[i++] = `[size]: ${ctx.stylize(`${value.size}`, 'number')}`;
  for (var n = 0; n < keys.length; n++) {
    output[i++] = formatProperty(ctx, value, recurseTimes, keys[n], 0);
  }
  return output;
}

function formatCollectionIterator(ctx, value, recurseTimes, keys) {
  ensureDebugIsInitialized();
  const mirror = Debug.MakeMirror(value, true);
  const vals = mirror.preview();
  const output = [];
  for (const o of vals) {
    output.push(formatValue(ctx, o, recurseTimes));
  }
  return output;
}

function formatPromise(ctx, value, recurseTimes, keys) {
  var output;
  const [state, result] = getPromiseDetails(value);
  if (state === kPending) {
    output = ['<pending>'];
  } else {
    const str = formatValue(ctx, result, recurseTimes);
    output = [state === kRejected ? `<rejected> ${str}` : str];
  }
  for (var n = 0; n < keys.length; n++) {
    output.push(formatProperty(ctx, value, recurseTimes, keys[n], 0));
  }
  return output;
}

function formatProperty(ctx, value, recurseTimes, key, array) {
  var name, str;
  const desc = Object.getOwnPropertyDescriptor(value, key) ||
    { value: value[key], enumerable: true };
  if (desc.value !== undefined) {
    const diff = array === 0 ? 3 : 2;
    ctx.indentationLvl += diff;
    str = formatValue(ctx, desc.value, recurseTimes, array === 0);
    ctx.indentationLvl -= diff;
  } else if (desc.get !== undefined) {
    if (desc.set !== undefined) {
      str = ctx.stylize('[Getter/Setter]', 'special');
    } else {
      str = ctx.stylize('[Getter]', 'special');
    }
  } else if (desc.set !== undefined) {
    str = ctx.stylize('[Setter]', 'special');
  } else {
    str = ctx.stylize('undefined', 'undefined');
  }
  if (array === 1) {
    return str;
  }
  if (typeof key === 'symbol') {
    name = `[${ctx.stylize(key.toString(), 'symbol')}]`;
  } else if (desc.enumerable === false) {
    name = `[${key}]`;
  } else if (keyStrRegExp.test(key)) {
    name = ctx.stylize(key, 'name');
  } else {
    name = ctx.stylize(keyEscape(key), 'string');
  }

  return `${name}: ${str}`;
}

function reduceToSingleString(ctx, output, base, braces, addLn) {
  const breakLength = ctx.breakLength;
  if (output.length * 2 <= breakLength) {
    var length = 0;
    for (var i = 0; i < output.length && length <= breakLength; i++) {
      if (ctx.colors) {
        length += output[i].replace(colorRegExp, '').length + 1;
      } else {
        length += output[i].length + 1;
      }
    }
    if (length <= breakLength)
      return `${braces[0]}${base} ${join(output, ', ')} ${braces[1]}`;
  }
  // If the opening "brace" is too large, like in the case of "Set {",
  // we need to force the first item to be on the next line or the
  // items will not line up correctly.
  const indentation = ' '.repeat(ctx.indentationLvl);
  const extraLn = addLn === true ? `\n${indentation}` : '';
  const ln = base === '' && braces[0].length === 1 ?
    ' ' : `${base}\n${indentation}  `;
  const str = join(output, `,\n${indentation}  `);
  return `${extraLn}${braces[0]}${ln}${str} ${braces[1]}`;
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
  return n < 10 ? `0${n.toString(10)}` : n.toString(10);
}

const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep',
                'Oct', 'Nov', 'Dec'];

// 26 Feb 16:19:34
function timestamp() {
  var d = new Date();
  var time = [pad(d.getHours()),
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
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'ctor', 'function');

  if (superCtor === undefined || superCtor === null)
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'superCtor', 'function');

  if (superCtor.prototype === undefined) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'superCtor.prototype',
                               'function');
  }
  ctor.super_ = superCtor;
  Object.setPrototypeOf(ctor.prototype, superCtor.prototype);
}

function _extend(target, source) {
  // Don't do anything if source isn't an object
  if (source === null || typeof source !== 'object') return target;

  var keys = Object.keys(source);
  var i = keys.length;
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

function _errnoException(err, syscall, original) {
  if (typeof err !== 'number' || err >= 0 || !Number.isSafeInteger(err)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'err',
                               'negative number');
  }
  const name = errname(err);
  var message = `${syscall} ${name}`;
  if (original)
    message += ` ${original}`;
  const e = new Error(message);
  e.code = e.errno = name;
  e.syscall = syscall;
  return e;
}

function _exceptionWithHostPort(err,
                                syscall,
                                address,
                                port,
                                additional) {
  var details;
  if (port && port > 0) {
    details = `${address}:${port}`;
  } else {
    details = address;
  }

  if (additional) {
    details += ` - Local (${additional})`;
  }
  var ex = exports._errnoException(err, syscall, details);
  ex.address = address;
  if (port) {
    ex.port = port;
  }
  return ex;
}

// process.versions needs a custom function as some values are lazy-evaluated.
process.versions[inspect.custom] =
  () => exports.format(JSON.parse(JSON.stringify(process.versions)));

function callbackifyOnRejected(reason, cb) {
  // `!reason` guard inspired by bluebird (Ref: https://goo.gl/t5IS6M).
  // Because `null` is a special error value in callbacks which means "no error
  // occurred", we error-wrap so the callback consumer can distinguish between
  // "the promise rejected with null" or "the promise fulfilled with undefined".
  if (!reason) {
    const newReason = new errors.Error('ERR_FALSY_VALUE_REJECTION');
    newReason.reason = reason;
    reason = newReason;
    Error.captureStackTrace(reason, callbackifyOnRejected);
  }
  return cb(reason);
}

function callbackify(original) {
  if (typeof original !== 'function') {
    throw new errors.TypeError(
      'ERR_INVALID_ARG_TYPE',
      'original',
      'function');
  }

  // We DO NOT return the promise as it gives the user a false sense that
  // the promise is actually somehow related to the callback's execution
  // and that the callback throwing will reject the promise.
  function callbackified(...args) {
    const maybeCb = args.pop();
    if (typeof maybeCb !== 'function') {
      throw new errors.TypeError(
        'ERR_INVALID_ARG_TYPE',
        'last argument',
        'function');
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

// Keep the `exports =` so that various functions can still be monkeypatched
module.exports = exports = {
  _errnoException,
  _exceptionWithHostPort,
  _extend,
  callbackify,
  debuglog,
  deprecate,
  format,
  inherits,
  inspect,
  isArray: Array.isArray,
  isBoolean,
  isBuffer,
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
