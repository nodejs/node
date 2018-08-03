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
const {
  ERR_FALSY_VALUE_REJECTION,
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE
} = errors.codes;
const { validateNumber } = require('internal/validators');
const { TextDecoder, TextEncoder } = require('internal/encoding');
const { isBuffer } = require('buffer').Buffer;

const {
  getPromiseDetails,
  getProxyDetails,
  kPending,
  kRejected,
  previewEntries
} = process.binding('util');

const { internalBinding } = require('internal/bootstrap/loaders');
const types = internalBinding('types');
Object.assign(types, require('internal/util/types'));
const {
  isAnyArrayBuffer,
  isArrayBuffer,
  isArgumentsObject,
  isDataView,
  isExternal,
  isMap,
  isMapIterator,
  isPromise,
  isSet,
  isSetIterator,
  isWeakMap,
  isWeakSet,
  isRegExp,
  isDate,
  isTypedArray,
  isStringObject,
  isNumberObject,
  isBooleanObject,
  isSymbolObject,
  isBigIntObject,
  isUint8Array,
  isUint8ClampedArray,
  isUint16Array,
  isUint32Array,
  isInt8Array,
  isInt16Array,
  isInt32Array,
  isFloat32Array,
  isFloat64Array,
  isBigInt64Array,
  isBigUint64Array
} = types;

const {
  customInspectSymbol,
  deprecate,
  getSystemErrorName: internalErrorName,
  isError,
  promisify,
  join,
  removeColors
} = require('internal/util');

const inspectDefaultOptions = Object.seal({
  showHidden: false,
  depth: 2,
  colors: false,
  customInspect: true,
  showProxy: false,
  maxArrayLength: 100,
  breakLength: 60,
  compact: true
});

const ReflectApply = Reflect.apply;

// This function is borrowed from the function with the same name on V8 Extras'
// `utils` object. V8 implements Reflect.apply very efficiently in conjunction
// with the spread syntax, such that no additional special case is needed for
// function calls w/o arguments.
// Refs: https://github.com/v8/v8/blob/d6ead37d265d7215cf9c5f768f279e21bd170212/src/js/prologue.js#L152-L156
function uncurryThis(func) {
  return (thisArg, ...args) => ReflectApply(func, thisArg, args);
}

const propertyIsEnumerable = uncurryThis(Object.prototype.propertyIsEnumerable);
const regExpToString = uncurryThis(RegExp.prototype.toString);
const dateToISOString = uncurryThis(Date.prototype.toISOString);
const errorToString = uncurryThis(Error.prototype.toString);

const bigIntValueOf = uncurryThis(BigInt.prototype.valueOf);
const booleanValueOf = uncurryThis(Boolean.prototype.valueOf);
const numberValueOf = uncurryThis(Number.prototype.valueOf);
const symbolValueOf = uncurryThis(Symbol.prototype.valueOf);
const stringValueOf = uncurryThis(String.prototype.valueOf);

const setValues = uncurryThis(Set.prototype.values);
const mapEntries = uncurryThis(Map.prototype.entries);
const dateGetTime = uncurryThis(Date.prototype.getTime);

let CIRCULAR_ERROR_MESSAGE;
let internalDeepEqual;

/* eslint-disable no-control-regex */
const strEscapeSequencesRegExp = /[\x00-\x1f\x27\x5c]/;
const strEscapeSequencesReplacer = /[\x00-\x1f\x27\x5c]/g;
/* eslint-enable no-control-regex */

const keyStrRegExp = /^[a-zA-Z_][a-zA-Z_0-9]*$/;
const numberRegExp = /^(0|[1-9][0-9]*)$/;

const readableRegExps = {};

const MIN_LINE_LENGTH = 16;

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
// Constants to map the iterator state.
const kWeak = 0;
const kIterator = 1;
const kMapEntries = 2;

const escapeFn = (str) => meta[str.charCodeAt(0)];

// Escape control characters, single quotes and the backslash.
// This is similar to JSON stringify escaping.
function strEscape(str) {
  // Some magic numbers that worked out fine while benchmarking with v8 6.0
  if (str.length < 5000 && !strEscapeSequencesRegExp.test(str))
    return `'${str}'`;
  if (str.length > 100)
    return `'${str.replace(strEscapeSequencesReplacer, escapeFn)}'`;
  let result = '';
  let last = 0;
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
            tempStr = `${Number(arguments[a++])}`;
            break;
          case 79: // 'O'
            tempStr = inspect(arguments[a++], inspectOptions);
            break;
          case 111: // 'o'
          {
            const opts = Object.assign({}, inspectOptions, {
              showHidden: true,
              showProxy: true,
              depth: 4
            });
            tempStr = inspect(arguments[a++], opts);
            break;
          }
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

function debuglog(set) {
  set = set.toUpperCase();
  if (!debugs[set]) {
    if (debugEnvRegex.test(set)) {
      const pid = process.pid;
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

/**
 * Echos the value of any input. Tries to print the value out
 * in the best way possible given the different types.
 *
 * @param {any} value The value to print out.
 * @param {Object} opts Optional options object that alters the output.
 */
/* Legacy: value, showHidden, depth, colors */
function inspect(value, opts) {
  // Default options
  const ctx = {
    seen: [],
    stylize: stylizeNoColor,
    showHidden: inspectDefaultOptions.showHidden,
    depth: inspectDefaultOptions.depth,
    colors: inspectDefaultOptions.colors,
    customInspect: inspectDefaultOptions.customInspect,
    showProxy: inspectDefaultOptions.showProxy,
    // TODO(BridgeAR): Deprecate `maxArrayLength` and replace it with
    // `maxEntries`.
    maxArrayLength: inspectDefaultOptions.maxArrayLength,
    breakLength: inspectDefaultOptions.breakLength,
    indentationLvl: 0,
    compact: inspectDefaultOptions.compact
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
  return formatValue(ctx, value, ctx.depth);
}
inspect.custom = customInspectSymbol;

Object.defineProperty(inspect, 'defaultOptions', {
  get() {
    return inspectDefaultOptions;
  },
  set(options) {
    if (options === null || typeof options !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
    }
    // TODO(BridgeAR): Add input validation and make sure `defaultOptions` are
    // not configurable.
    return _extend(inspectDefaultOptions, options);
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
  'bigint': 'yellow',
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

function getConstructorName(obj) {
  while (obj) {
    const descriptor = Object.getOwnPropertyDescriptor(obj, 'constructor');
    if (descriptor !== undefined &&
        typeof descriptor.value === 'function' &&
        descriptor.value.name !== '') {
      return descriptor.value.name;
    }

    obj = Object.getPrototypeOf(obj);
  }

  return '';
}

function getPrefix(constructor, tag, fallback) {
  if (constructor !== '') {
    if (tag !== '' && constructor !== tag) {
      return `${constructor} [${tag}] `;
    }
    return `${constructor} `;
  }

  if (tag !== '')
    return `[${tag}] `;

  if (fallback !== undefined)
    return `${fallback} `;

  return '';
}

function findTypedConstructor(value) {
  for (const [check, clazz] of [
    [isUint8Array, Uint8Array],
    [isUint8ClampedArray, Uint8ClampedArray],
    [isUint16Array, Uint16Array],
    [isUint32Array, Uint32Array],
    [isInt8Array, Int8Array],
    [isInt16Array, Int16Array],
    [isInt32Array, Int32Array],
    [isFloat32Array, Float32Array],
    [isFloat64Array, Float64Array],
    [isBigInt64Array, BigInt64Array],
    [isBigUint64Array, BigUint64Array]
  ]) {
    if (check(value)) {
      return clazz;
    }
  }
}

const getBoxedValue = formatPrimitive.bind(null, stylizeNoColor);

function noPrototypeIterator(ctx, value, recurseTimes) {
  let newVal;
  // TODO: Create a Subclass in case there's no prototype and show
  // `null-prototype`.
  if (isSet(value)) {
    const clazz = Object.getPrototypeOf(value) || Set;
    newVal = new clazz(setValues(value));
  } else if (isMap(value)) {
    const clazz = Object.getPrototypeOf(value) || Map;
    newVal = new clazz(mapEntries(value));
  } else if (Array.isArray(value)) {
    const clazz = Object.getPrototypeOf(value) || Array;
    newVal = new clazz(value.length || 0);
  } else if (isTypedArray(value)) {
    const clazz = findTypedConstructor(value) || Uint8Array;
    newVal = new clazz(value);
  }
  if (newVal) {
    Object.defineProperties(newVal, Object.getOwnPropertyDescriptors(value));
    return formatValue(ctx, newVal, recurseTimes);
  }
}

// Note: using `formatValue` directly requires the indentation level to be
// corrected by setting `ctx.indentationLvL += diff` and then to decrease the
// value afterwards again.
function formatValue(ctx, value, recurseTimes) {
  // Primitive types cannot have properties
  if (typeof value !== 'object' && typeof value !== 'function') {
    return formatPrimitive(ctx.stylize, value, ctx);
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
    let maybeCustom = value[customInspectSymbol];

    if (!maybeCustom && value.inspect !== exports.inspect &&
        typeof value.inspect === 'function') {
      maybeCustom = deprecate(
        value.inspect,
        'Custom inspection function on Objects via .inspect() is deprecated',
        'DEP0079'
      );
    }

    if (typeof maybeCustom === 'function' &&
        // Filter out the util module, its inspect function is special
        maybeCustom !== exports.inspect &&
        // Also filter out any prototype objects using the circular check.
        !(value.constructor && value.constructor.prototype === value)) {
      const ret = maybeCustom.call(value, recurseTimes, ctx);

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

  // Using an array here is actually better for the average case than using
  // a Set. `seen` will only check for the depth and will never grow too large.
  if (ctx.seen.indexOf(value) !== -1)
    return ctx.stylize('[Circular]', 'special');

  let keys;
  let symbols = Object.getOwnPropertySymbols(value);

  // Look up the keys of the object.
  if (ctx.showHidden) {
    keys = Object.getOwnPropertyNames(value);
  } else {
    // This might throw if `value` is a Module Namespace Object from an
    // unevaluated module, but we don't want to perform the actual type
    // check because it's expensive.
    // TODO(devsnek): track https://github.com/tc39/ecma262/issues/1209
    // and modify this logic as needed.
    try {
      keys = Object.keys(value);
    } catch (err) {
      if (types.isNativeError(err) &&
          err.name === 'ReferenceError' &&
          types.isModuleNamespaceObject(value)) {
        keys = Object.getOwnPropertyNames(value);
      } else {
        throw err;
      }
    }

    if (symbols.length !== 0)
      symbols = symbols.filter((key) => propertyIsEnumerable(value, key));
  }

  const keyLength = keys.length + symbols.length;

  const constructor = getConstructorName(value);
  let tag = value[Symbol.toStringTag];
  if (typeof tag !== 'string')
    tag = '';
  let base = '';
  let formatter = formatObject;
  let braces;
  let noIterator = true;
  let extra;
  let i = 0;

  // Iterators and the rest are split to reduce checks
  if (value[Symbol.iterator]) {
    noIterator = false;
    if (Array.isArray(value)) {
      // Only set the constructor for non ordinary ("Array [...]") arrays.
      const prefix = getPrefix(constructor, tag);
      braces = [`${prefix === 'Array ' ? '' : prefix}[`, ']'];
      if (value.length === 0 && keyLength === 0)
        return `${braces[0]}]`;
      formatter = formatArray;
    } else if (isSet(value)) {
      const prefix = getPrefix(constructor, tag);
      if (value.size === 0 && keyLength === 0)
        return `${prefix}{}`;
      braces = [`${prefix}{`, '}'];
      formatter = formatSet;
    } else if (isMap(value)) {
      const prefix = getPrefix(constructor, tag);
      if (value.size === 0 && keyLength === 0)
        return `${prefix}{}`;
      braces = [`${prefix}{`, '}'];
      formatter = formatMap;
    } else if (isTypedArray(value)) {
      braces = [`${getPrefix(constructor, tag)}[`, ']'];
      if (value.length === 0 && keyLength === 0 && !ctx.showHidden)
        return `${braces[0]}]`;
      formatter = formatTypedArray;
    } else if (isMapIterator(value)) {
      braces = [`[${tag}] {`, '}'];
      formatter = formatMapIterator;
    } else if (isSetIterator(value)) {
      braces = [`[${tag}] {`, '}'];
      formatter = formatSetIterator;
    } else {
      noIterator = true;
    }
  }
  if (noIterator) {
    braces = ['{', '}'];
    if (constructor === 'Object') {
      if (isArgumentsObject(value)) {
        if (keyLength === 0)
          return '[Arguments] {}';
        braces[0] = '[Arguments] {';
      } else if (tag !== '') {
        braces[0] = `${getPrefix(constructor, tag)}{`;
        if (keyLength === 0) {
          return `${braces[0]}}`;
        }
      } else if (keyLength === 0) {
        return '{}';
      }
    } else if (typeof value === 'function') {
      const type = constructor || tag || 'Function';
      const name = `${type}${value.name ? `: ${value.name}` : ''}`;
      if (keyLength === 0)
        return ctx.stylize(`[${name}]`, 'special');
      base = `[${name}]`;
    } else if (isRegExp(value)) {
      // Make RegExps say that they are RegExps
      if (keyLength === 0 || recurseTimes < 0)
        return ctx.stylize(regExpToString(value), 'regexp');
      base = `${regExpToString(value)}`;
    } else if (isDate(value)) {
      // Make dates with properties first say the date
      if (keyLength === 0) {
        if (Number.isNaN(dateGetTime(value)))
          return ctx.stylize(String(value), 'date');
        return ctx.stylize(dateToISOString(value), 'date');
      }
      base = dateToISOString(value);
    } else if (isError(value)) {
      // Make error with message first say the error
      base = formatError(value);
      // Wrap the error in brackets in case it has no stack trace.
      const stackStart = base.indexOf('\n    at');
      if (stackStart === -1) {
        base = `[${base}]`;
      }
      // The message and the stack have to be indented as well!
      if (ctx.indentationLvl !== 0) {
        const indentation = ' '.repeat(ctx.indentationLvl);
        base = formatError(value).replace(/\n/g, `\n${indentation}`);
      }
      if (keyLength === 0)
        return base;

      if (ctx.compact === false && stackStart !== -1) {
        braces[0] += `${base.slice(stackStart)}`;
        base = `[${base.slice(0, stackStart)}]`;
      }
    } else if (isAnyArrayBuffer(value)) {
      // Fast path for ArrayBuffer and SharedArrayBuffer.
      // Can't do the same for DataView because it has a non-primitive
      // .buffer property that we need to recurse for.
      let prefix = getPrefix(constructor, tag);
      if (prefix === '') {
        prefix = isArrayBuffer(value) ? 'ArrayBuffer ' : 'SharedArrayBuffer ';
      }
      if (keyLength === 0)
        return prefix +
              `{ byteLength: ${formatNumber(ctx.stylize, value.byteLength)} }`;
      braces[0] = `${prefix}{`;
      keys.unshift('byteLength');
    } else if (isDataView(value)) {
      braces[0] = `${getPrefix(constructor, tag, 'DataView')}{`;
      // .buffer goes last, it's not a primitive like the others.
      keys.unshift('byteLength', 'byteOffset', 'buffer');
    } else if (isPromise(value)) {
      braces[0] = `${getPrefix(constructor, tag, 'Promise')}{`;
      formatter = formatPromise;
    } else if (isWeakSet(value)) {
      braces[0] = `${getPrefix(constructor, tag, 'WeakSet')}{`;
      if (ctx.showHidden) {
        formatter = formatWeakSet;
      } else {
        extra = ctx.stylize('[items unknown]', 'special');
      }
    } else if (isWeakMap(value)) {
      braces[0] = `${getPrefix(constructor, tag, 'WeakMap')}{`;
      if (ctx.showHidden) {
        formatter = formatWeakMap;
      } else {
        extra = ctx.stylize('[items unknown]', 'special');
      }
    } else if (types.isModuleNamespaceObject(value)) {
      braces[0] = `[${tag}] {`;
      formatter = formatNamespaceObject;
    } else if (isNumberObject(value)) {
      base = `[Number: ${getBoxedValue(numberValueOf(value))}]`;
      if (keyLength === 0)
        return ctx.stylize(base, 'number');
    } else if (isBooleanObject(value)) {
      base = `[Boolean: ${getBoxedValue(booleanValueOf(value))}]`;
      if (keyLength === 0)
        return ctx.stylize(base, 'boolean');
    } else if (isBigIntObject(value)) {
      base = `[BigInt: ${getBoxedValue(bigIntValueOf(value))}]`;
      if (keyLength === 0)
        return ctx.stylize(base, 'bigint');
    } else if (isSymbolObject(value)) {
      base = `[Symbol: ${getBoxedValue(symbolValueOf(value))}]`;
      if (keyLength === 0)
        return ctx.stylize(base, 'symbol');
    } else if (isStringObject(value)) {
      const raw = stringValueOf(value);
      base = `[String: ${getBoxedValue(raw, ctx)}]`;
      if (keyLength === raw.length)
        return ctx.stylize(base, 'string');
      // For boxed Strings, we have to remove the 0-n indexed entries,
      // since they just noisy up the output and are redundant
      // Make boxed primitive Strings look like such
      keys = keys.slice(value.length);
      braces = ['{', '}'];
    // The input prototype got manipulated. Special handle these.
    // We have to rebuild the information so we are able to display everything.
    } else {
      const specialIterator = noPrototypeIterator(ctx, value, recurseTimes);
      if (specialIterator) {
        return specialIterator;
      }
      if (isMapIterator(value)) {
        braces = [`[${tag || 'Map Iterator'}] {`, '}'];
        formatter = formatMapIterator;
      } else if (isSetIterator(value)) {
        braces = [`[${tag || 'Set Iterator'}] {`, '}'];
        formatter = formatSetIterator;
      // Handle other regular objects again.
      } else if (keyLength === 0) {
        if (isExternal(value))
          return ctx.stylize('[External]', 'special');
        return `${getPrefix(constructor, tag)}{}`;
      } else {
        braces[0] = `${getPrefix(constructor, tag)}{`;
      }
    }
  }

  if (recurseTimes != null) {
    if (recurseTimes < 0)
      return ctx.stylize(`[${constructor || tag || 'Object'}]`, 'special');
    recurseTimes -= 1;
  }

  ctx.seen.push(value);
  let output;
  // This corresponds to a depth of at least 333 and likely 500.
  if (ctx.indentationLvl < 1000) {
    output = formatter(ctx, value, recurseTimes, keys);
  } else {
    try {
      output = formatter(ctx, value, recurseTimes, keys);
    } catch (err) {
      if (errors.isStackOverflowError(err)) {
        ctx.seen.pop();
        return ctx.stylize(
          `[${constructor || tag || 'Object'}: Inspection interrupted ` +
            'prematurely. Maximum call stack size exceeded.]',
          'special'
        );
      }
      throw err;
    }
  }
  if (extra !== undefined)
    output.unshift(extra);

  for (i = 0; i < symbols.length; i++) {
    output.push(formatProperty(ctx, value, recurseTimes, symbols[i], 0));
  }

  ctx.seen.pop();

  return reduceToSingleString(ctx, output, base, braces);
}

function formatNumber(fn, value) {
  // Format -0 as '-0'. Checking `value === -0` won't distinguish 0 from -0.
  if (Object.is(value, -0))
    return fn('-0', 'number');
  return fn(`${value}`, 'number');
}

function formatPrimitive(fn, value, ctx) {
  if (typeof value === 'string') {
    if (ctx.compact === false &&
      ctx.indentationLvl + value.length > ctx.breakLength &&
      value.length > MIN_LINE_LENGTH) {
      // eslint-disable-next-line max-len
      const minLineLength = Math.max(ctx.breakLength - ctx.indentationLvl, MIN_LINE_LENGTH);
      // eslint-disable-next-line max-len
      const averageLineLength = Math.ceil(value.length / Math.ceil(value.length / minLineLength));
      const divisor = Math.max(averageLineLength, MIN_LINE_LENGTH);
      let res = '';
      if (readableRegExps[divisor] === undefined) {
        // Build a new RegExp that naturally breaks text into multiple lines.
        //
        // Rules
        // 1. Greedy match all text up the max line length that ends with a
        //    whitespace or the end of the string.
        // 2. If none matches, non-greedy match any text up to a whitespace or
        //    the end of the string.
        //
        // eslint-disable-next-line max-len, node-core/no-unescaped-regexp-dot
        readableRegExps[divisor] = new RegExp(`(.|\\n){1,${divisor}}(\\s|$)|(\\n|.)+?(\\s|$)`, 'gm');
      }
      const matches = value.match(readableRegExps[divisor]);
      if (matches.length > 1) {
        const indent = ' '.repeat(ctx.indentationLvl);
        res += `${fn(strEscape(matches[0]), 'string')} +\n`;
        for (var i = 1; i < matches.length - 1; i++) {
          res += `${indent}  ${fn(strEscape(matches[i]), 'string')} +\n`;
        }
        res += `${indent}  ${fn(strEscape(matches[i]), 'string')}`;
        return res;
      }
    }
    return fn(strEscape(value), 'string');
  }
  if (typeof value === 'number')
    return formatNumber(fn, value);
  // eslint-disable-next-line valid-typeof
  if (typeof value === 'bigint')
    return fn(`${value}n`, 'bigint');
  if (typeof value === 'boolean')
    return fn(`${value}`, 'boolean');
  if (typeof value === 'undefined')
    return fn('undefined', 'undefined');
  // es6 symbol primitive
  return fn(value.toString(), 'symbol');
}

function formatError(value) {
  return value.stack || errorToString(value);
}

function formatObject(ctx, value, recurseTimes, keys) {
  const len = keys.length;
  const output = new Array(len);
  for (var i = 0; i < len; i++)
    output[i] = formatProperty(ctx, value, recurseTimes, keys[i], 0);
  return output;
}

function formatNamespaceObject(ctx, value, recurseTimes, keys) {
  const len = keys.length;
  const output = new Array(len);
  for (var i = 0; i < len; i++) {
    try {
      output[i] = formatProperty(ctx, value, recurseTimes, keys[i], 0);
    } catch (err) {
      if (!(types.isNativeError(err) && err.name === 'ReferenceError')) {
        throw err;
      }
      // Use the existing functionality. This makes sure the indentation and
      // line breaks are always correct. Otherwise it is very difficult to keep
      // this aligned, even though this is a hacky way of dealing with this.
      const tmp = { [keys[i]]: '' };
      output[i] = formatProperty(ctx, tmp, recurseTimes, keys[i], 0);
      const pos = output[i].lastIndexOf(' ');
      // We have to find the last whitespace and have to replace that value as
      // it will be visualized as a regular string.
      output[i] = output[i].slice(0, pos + 1) +
                  ctx.stylize('<uninitialized>', 'special');
    }
  }
  return output;
}

// The array is sparse and/or has extra keys
function formatSpecialArray(ctx, value, recurseTimes, keys, maxLength, valLen) {
  const output = [];
  const keyLen = keys.length;
  let i = 0;
  for (const key of keys) {
    if (output.length === maxLength)
      break;
    const index = +key;
    // Arrays can only have up to 2^32 - 1 entries
    if (index > 2 ** 32 - 2)
      break;
    if (`${i}` !== key) {
      if (!numberRegExp.test(key))
        break;
      const emptyItems = index - i;
      const ending = emptyItems > 1 ? 's' : '';
      const message = `<${emptyItems} empty item${ending}>`;
      output.push(ctx.stylize(message, 'undefined'));
      i = index;
      if (output.length === maxLength)
        break;
    }
    output.push(formatProperty(ctx, value, recurseTimes, key, 1));
    i++;
  }
  if (i < valLen && output.length !== maxLength) {
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
    let key;
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
    ctx.indentationLvl += 2;
    for (const key of [
      'BYTES_PER_ELEMENT',
      'length',
      'byteLength',
      'byteOffset',
      'buffer'
    ]) {
      const str = formatValue(ctx, value[key], recurseTimes);
      output.push(`[${key}]: ${str}`);
    }
    ctx.indentationLvl -= 2;
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
  let i = 0;
  ctx.indentationLvl += 2;
  for (const v of value) {
    output[i++] = formatValue(ctx, v, recurseTimes);
  }
  ctx.indentationLvl -= 2;
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
  let i = 0;
  ctx.indentationLvl += 2;
  for (const [k, v] of value) {
    output[i++] = `${formatValue(ctx, k, recurseTimes)} => ` +
                  formatValue(ctx, v, recurseTimes);
  }
  ctx.indentationLvl -= 2;
  // See comment in formatSet
  if (ctx.showHidden)
    output[i++] = `[size]: ${ctx.stylize(`${value.size}`, 'number')}`;
  for (var n = 0; n < keys.length; n++) {
    output[i++] = formatProperty(ctx, value, recurseTimes, keys[n], 0);
  }
  return output;
}

function formatSetIterInner(ctx, value, recurseTimes, keys, entries, state) {
  const maxArrayLength = Math.max(ctx.maxArrayLength, 0);
  const maxLength = Math.min(maxArrayLength, entries.length);
  let output = new Array(maxLength);
  ctx.indentationLvl += 2;
  for (var i = 0; i < maxLength; i++) {
    output[i] = formatValue(ctx, entries[i], recurseTimes);
  }
  ctx.indentationLvl -= 2;
  if (state === kWeak) {
    // Sort all entries to have a halfway reliable output (if more entries than
    // retrieved ones exist, we can not reliably return the same output).
    output = output.sort();
  }
  const remaining = entries.length - maxLength;
  if (remaining > 0) {
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  }
  for (i = 0; i < keys.length; i++)
    output.push(formatProperty(ctx, value, recurseTimes, keys[i], 0));
  return output;
}

function formatMapIterInner(ctx, value, recurseTimes, keys, entries, state) {
  const maxArrayLength = Math.max(ctx.maxArrayLength, 0);
  // Entries exist as [key1, val1, key2, val2, ...]
  const len = entries.length / 2;
  const remaining = len - maxArrayLength;
  const maxLength = Math.min(maxArrayLength, len);
  let output = new Array(maxLength);
  let start = '';
  let end = '';
  let middle = ' => ';
  let i = 0;
  if (state === kMapEntries) {
    start = '[ ';
    end = ' ]';
    middle = ', ';
  }
  ctx.indentationLvl += 2;
  for (; i < maxLength; i++) {
    const pos = i * 2;
    output[i] = `${start}${formatValue(ctx, entries[pos], recurseTimes)}` +
      `${middle}${formatValue(ctx, entries[pos + 1], recurseTimes)}${end}`;
  }
  ctx.indentationLvl -= 2;
  if (state === kWeak) {
    // Sort all entries to have a halfway reliable output (if more entries
    // than retrieved ones exist, we can not reliably return the same output).
    output = output.sort();
  }
  if (remaining > 0) {
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  }
  for (i = 0; i < keys.length; i++)
    output.push(formatProperty(ctx, value, recurseTimes, keys[i], 0));
  return output;
}

function formatWeakSet(ctx, value, recurseTimes, keys) {
  const entries = previewEntries(value);
  return formatSetIterInner(ctx, value, recurseTimes, keys, entries, kWeak);
}

function formatWeakMap(ctx, value, recurseTimes, keys) {
  const entries = previewEntries(value);
  return formatMapIterInner(ctx, value, recurseTimes, keys, entries, kWeak);
}

function formatSetIterator(ctx, value, recurseTimes, keys) {
  const entries = previewEntries(value);
  return formatSetIterInner(ctx, value, recurseTimes, keys, entries, kIterator);
}

function formatMapIterator(ctx, value, recurseTimes, keys) {
  const [entries, isKeyValue] = previewEntries(value, true);
  if (isKeyValue) {
    return formatMapIterInner(
      ctx, value, recurseTimes, keys, entries, kMapEntries);
  }

  return formatSetIterInner(ctx, value, recurseTimes, keys, entries, kIterator);
}

function formatPromise(ctx, value, recurseTimes, keys) {
  let output;
  const [state, result] = getPromiseDetails(value);
  if (state === kPending) {
    output = [ctx.stylize('<pending>', 'special')];
  } else {
    // Using `formatValue` is correct here without the need to fix the
    // indentation level.
    ctx.indentationLvl += 2;
    const str = formatValue(ctx, result, recurseTimes);
    ctx.indentationLvl -= 2;
    output = [
      state === kRejected ?
        `${ctx.stylize('<rejected>', 'special')} ${str}` :
        str
    ];
  }
  for (var n = 0; n < keys.length; n++) {
    output.push(formatProperty(ctx, value, recurseTimes, keys[n], 0));
  }
  return output;
}

function formatProperty(ctx, value, recurseTimes, key, array) {
  let name, str;
  let extra = ' ';
  const desc = Object.getOwnPropertyDescriptor(value, key) ||
    { value: value[key], enumerable: true };
  if (desc.value !== undefined) {
    const diff = array !== 0 || ctx.compact === false ? 2 : 3;
    ctx.indentationLvl += diff;
    str = formatValue(ctx, desc.value, recurseTimes);
    if (diff === 3) {
      const len = ctx.colors ? removeColors(str).length : str.length;
      if (ctx.breakLength < len) {
        extra = `\n${' '.repeat(ctx.indentationLvl)}`;
      }
    }
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
    const tmp = key.toString().replace(strEscapeSequencesReplacer, escapeFn);
    name = `[${ctx.stylize(tmp, 'symbol')}]`;
  } else if (desc.enumerable === false) {
    name = `[${key.replace(strEscapeSequencesReplacer, escapeFn)}]`;
  } else if (keyStrRegExp.test(key)) {
    name = ctx.stylize(key, 'name');
  } else {
    name = ctx.stylize(strEscape(key), 'string');
  }
  return `${name}:${extra}${str}`;
}

function reduceToSingleString(ctx, output, base, braces) {
  const breakLength = ctx.breakLength;
  let i = 0;
  if (ctx.compact === false) {
    const indentation = ' '.repeat(ctx.indentationLvl);
    let res = `${base ? `${base} ` : ''}${braces[0]}\n${indentation}  `;
    for (; i < output.length - 1; i++) {
      res += `${output[i]},\n${indentation}  `;
    }
    res += `${output[i]}\n${indentation}${braces[1]}`;
    return res;
  }
  if (output.length * 2 <= breakLength) {
    let length = 0;
    for (; i < output.length && length <= breakLength; i++) {
      if (ctx.colors) {
        length += removeColors(output[i]).length + 1;
      } else {
        length += output[i].length + 1;
      }
    }
    if (length <= breakLength)
      return `${braces[0]}${base ? ` ${base}` : ''} ${join(output, ', ')} ` +
        braces[1];
  }
  // If the opening "brace" is too large, like in the case of "Set {",
  // we need to force the first item to be on the next line or the
  // items will not line up correctly.
  const indentation = ' '.repeat(ctx.indentationLvl);
  const ln = base === '' && braces[0].length === 1 ?
    ' ' : `${base ? ` ${base}` : ''}\n${indentation}  `;
  const str = join(output, `,\n${indentation}  `);
  return `${braces[0]}${ln}${str} ${braces[1]}`;
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
  ctor.super_ = superCtor;
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
