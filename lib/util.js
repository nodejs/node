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
  isTypedArray,
  isRegExp: _isRegExp,
  isDate: _isDate,
  kPending,
  kRejected,
} = process.binding('util');

const {
  customInspectSymbol,
  deprecate,
  getConstructorOf,
  isError,
  promisify
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

const numbersOnlyRE = /^\d+$/;

const propertyIsEnumerable = Object.prototype.propertyIsEnumerable;
const regExpToString = RegExp.prototype.toString;
const dateToISOString = Date.prototype.toISOString;
const errorToString = Error.prototype.toString;

var CIRCULAR_ERROR_MESSAGE;
var Debug;

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
  if (typeof f !== 'string') {
    const objects = new Array(arguments.length);
    for (var index = 0; index < arguments.length; index++) {
      objects[index] = inspect(arguments[index]);
    }
    return objects.join(' ');
  }

  if (arguments.length === 1) return f;

  var str = '';
  var a = 1;
  var lastPos = 0;
  for (var i = 0; i < f.length;) {
    if (f.charCodeAt(i) === 37/*'%'*/ && i + 1 < f.length) {
      if (f.charCodeAt(i + 1) !== 37/*'%'*/ && a >= arguments.length) {
        ++i;
        continue;
      }
      switch (f.charCodeAt(i + 1)) {
        case 100: // 'd'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += Number(arguments[a++]);
          break;
        case 105: // 'i'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += parseInt(arguments[a++]);
          break;
        case 102: // 'f'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += parseFloat(arguments[a++]);
          break;
        case 106: // 'j'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += tryStringify(arguments[a++]);
          break;
        case 115: // 's'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += String(arguments[a++]);
          break;
        case 79: // 'O'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += inspect(arguments[a++]);
          break;
        case 111: // 'o'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += inspect(arguments[a++],
                         { showHidden: true, depth: 4, showProxy: true });
          break;
        case 37: // '%'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += '%';
          break;
        default: // any other character is not a correct placeholder
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += '%';
          lastPos = i = i + 1;
          continue;
      }
      lastPos = i = i + 2;
      continue;
    }
    ++i;
  }
  if (lastPos === 0)
    str = f;
  else if (lastPos < f.length)
    str += f.slice(lastPos);
  while (a < arguments.length) {
    const x = arguments[a++];
    if (x === null || (typeof x !== 'object' && typeof x !== 'symbol')) {
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
/* legacy: obj, showHidden, depth, colors*/
function inspect(obj, opts) {
  // default options
  var ctx = {
    seen: [],
    stylize: stylizeNoColor
  };
  // legacy...
  if (arguments.length >= 3 && arguments[2] !== undefined) {
    ctx.depth = arguments[2];
  }
  if (arguments.length >= 4 && arguments[3] !== undefined) {
    ctx.colors = arguments[3];
  }
  if (typeof opts === 'boolean') {
    // legacy...
    ctx.showHidden = opts;
  }
  // Set default and user-specified options
  ctx = Object.assign({}, inspect.defaultOptions, ctx, opts);
  if (ctx.colors) ctx.stylize = stylizeWithColor;
  if (ctx.maxArrayLength === null) ctx.maxArrayLength = Infinity;
  return formatValue(ctx, obj, ctx.depth);
}
inspect.custom = customInspectSymbol;

Object.defineProperty(inspect, 'defaultOptions', {
  get: function() {
    return inspectDefaultOptions;
  },
  set: function(options) {
    if (options === null || typeof options !== 'object') {
      throw new TypeError('"options" must be an object');
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
  var style = inspect.styles[styleType];

  if (style) {
    return `\u001b[${inspect.colors[style][0]}m${str}` +
           `\u001b[${inspect.colors[style][1]}m`;
  } else {
    return str;
  }
}


function stylizeNoColor(str, styleType) {
  return str;
}


function arrayToHash(array) {
  var hash = Object.create(null);

  for (var i = 0; i < array.length; i++) {
    var val = array[i];
    hash[val] = true;
  }

  return hash;
}


function ensureDebugIsInitialized() {
  if (Debug === undefined) {
    const runInDebugContext = require('vm').runInDebugContext;
    Debug = runInDebugContext('Debug');
  }
}


function formatValue(ctx, value, recurseTimes) {
  if (ctx.showProxy &&
      ((typeof value === 'object' && value !== null) ||
       typeof value === 'function')) {
    var proxy = undefined;
    var proxyCache = ctx.proxyCache;
    if (!proxyCache)
      proxyCache = ctx.proxyCache = new Map();
    // Determine if we've already seen this object and have
    // determined that it either is or is not a proxy.
    if (proxyCache.has(value)) {
      // We've seen it, if the value is not undefined, it's a Proxy.
      proxy = proxyCache.get(value);
    } else {
      // Haven't seen it. Need to check.
      // If it's not a Proxy, this will return undefined.
      // Otherwise, it'll return an array. The first item
      // is the target, the second item is the handler.
      // We ignore (and do not return) the Proxy isRevoked property.
      proxy = getProxyDetails(value);
      if (proxy) {
        // We know for a fact that this isn't a Proxy.
        // Mark it as having already been evaluated.
        // We do this because this object is passed
        // recursively to formatValue below in order
        // for it to get proper formatting, and because
        // the target and handle objects also might be
        // proxies... it's unfortunate but necessary.
        proxyCache.set(proxy, undefined);
      }
      // If the object is not a Proxy, then this stores undefined.
      // This tells the code above that we've already checked and
      // ruled it out. If the object is a proxy, this caches the
      // results of the getProxyDetails call.
      proxyCache.set(value, proxy);
    }
    if (proxy) {
      return `Proxy ${formatValue(ctx, proxy, recurseTimes)}`;
    }
  }

  // Provide a hook for user-specified inspect functions.
  // Check that value is an object with an inspect function on it
  if (ctx.customInspect && value) {
    const maybeCustomInspect = value[customInspectSymbol] || value.inspect;

    if (typeof maybeCustomInspect === 'function' &&
        // Filter out the util module, its inspect function is special
        maybeCustomInspect !== exports.inspect &&
        // Also filter out any prototype objects using the circular check.
        !(value.constructor && value.constructor.prototype === value)) {
      let ret = maybeCustomInspect.call(value, recurseTimes, ctx);

      // If the custom inspection method returned `this`, don't go into
      // infinite recursion.
      if (ret !== value) {
        if (typeof ret !== 'string') {
          ret = formatValue(ctx, ret, recurseTimes);
        }
        return ret;
      }
    }
  }

  // Primitive types cannot have properties
  var primitive = formatPrimitive(ctx, value);
  if (primitive) {
    return primitive;
  }

  // Look up the keys of the object.
  var keys = Object.keys(value);
  var visibleKeys = arrayToHash(keys);
  const symbolKeys = Object.getOwnPropertySymbols(value);
  const enumSymbolKeys = symbolKeys
      .filter((key) => propertyIsEnumerable.call(value, key));
  keys = keys.concat(enumSymbolKeys);

  if (ctx.showHidden) {
    keys = Object.getOwnPropertyNames(value).concat(symbolKeys);
  }

  // This could be a boxed primitive (new String(), etc.), check valueOf()
  // NOTE: Avoid calling `valueOf` on `Date` instance because it will return
  // a number which, when object has some additional user-stored `keys`,
  // will be printed out.
  var formatted;
  var raw = value;
  try {
    // the .valueOf() call can fail for a multitude of reasons
    if (!isDate(value))
      raw = value.valueOf();
  } catch (e) {
    // ignore...
  }

  if (typeof raw === 'string') {
    // for boxed Strings, we have to remove the 0-n indexed entries,
    // since they just noisy up the output and are redundant
    keys = keys.filter(function(key) {
      if (typeof key === 'symbol') {
        return true;
      }

      return !(key >= 0 && key < raw.length);
    });
  }

  var constructor = getConstructorOf(value);

  // Some type of object without properties can be shortcutted.
  if (keys.length === 0) {
    if (typeof value === 'function') {
      const ctorName = constructor ? constructor.name : 'Function';
      return ctx.stylize(
        `[${ctorName}${value.name ? `: ${value.name}` : ''}]`, 'special');
    }
    if (isRegExp(value)) {
      return ctx.stylize(regExpToString.call(value), 'regexp');
    }
    if (isDate(value)) {
      if (Number.isNaN(value.getTime())) {
        return ctx.stylize(value.toString(), 'date');
      } else {
        return ctx.stylize(dateToISOString.call(value), 'date');
      }
    }
    if (isError(value)) {
      return formatError(value);
    }
    // now check the `raw` value to handle boxed primitives
    if (typeof raw === 'string') {
      formatted = formatPrimitiveNoColor(ctx, raw);
      return ctx.stylize(`[String: ${formatted}]`, 'string');
    }
    if (typeof raw === 'symbol') {
      formatted = formatPrimitiveNoColor(ctx, raw);
      return ctx.stylize(`[Symbol: ${formatted}]`, 'symbol');
    }
    if (typeof raw === 'number') {
      formatted = formatPrimitiveNoColor(ctx, raw);
      return ctx.stylize(`[Number: ${formatted}]`, 'number');
    }
    if (typeof raw === 'boolean') {
      formatted = formatPrimitiveNoColor(ctx, raw);
      return ctx.stylize(`[Boolean: ${formatted}]`, 'boolean');
    }
    // Fast path for ArrayBuffer and SharedArrayBuffer.
    // Can't do the same for DataView because it has a non-primitive
    // .buffer property that we need to recurse for.
    if (isAnyArrayBuffer(value)) {
      return `${constructor.name}` +
             ` { byteLength: ${formatNumber(ctx, value.byteLength)} }`;
    }
    if (isExternal(value)) {
      return ctx.stylize('[External]', 'special');
    }
  }

  var base = '';
  var empty = false;
  var formatter = formatObject;
  var braces;

  // We can't compare constructors for various objects using a comparison like
  // `constructor === Array` because the object could have come from a different
  // context and thus the constructor won't match. Instead we check the
  // constructor names (including those up the prototype chain where needed) to
  // determine object types.
  if (Array.isArray(value)) {
    // Unset the constructor to prevent "Array [...]" for ordinary arrays.
    if (constructor && constructor.name === 'Array')
      constructor = null;
    braces = ['[', ']'];
    empty = value.length === 0;
    formatter = formatArray;
  } else if (isSet(value)) {
    braces = ['{', '}'];
    // With `showHidden`, `length` will display as a hidden property for
    // arrays. For consistency's sake, do the same for `size`, even though this
    // property isn't selected by Object.getOwnPropertyNames().
    if (ctx.showHidden)
      keys.unshift('size');
    empty = value.size === 0;
    formatter = formatSet;
  } else if (isMap(value)) {
    braces = ['{', '}'];
    // Ditto.
    if (ctx.showHidden)
      keys.unshift('size');
    empty = value.size === 0;
    formatter = formatMap;
  } else if (isAnyArrayBuffer(value)) {
    braces = ['{', '}'];
    keys.unshift('byteLength');
    visibleKeys.byteLength = true;
  } else if (isDataView(value)) {
    braces = ['{', '}'];
    // .buffer goes last, it's not a primitive like the others.
    keys.unshift('byteLength', 'byteOffset', 'buffer');
    visibleKeys.byteLength = true;
    visibleKeys.byteOffset = true;
    visibleKeys.buffer = true;
  } else if (isTypedArray(value)) {
    braces = ['[', ']'];
    formatter = formatTypedArray;
    if (ctx.showHidden) {
      // .buffer goes last, it's not a primitive like the others.
      keys.unshift('BYTES_PER_ELEMENT',
                   'length',
                   'byteLength',
                   'byteOffset',
                   'buffer');
    }
  } else if (isPromise(value)) {
    braces = ['{', '}'];
    formatter = formatPromise;
  } else if (isMapIterator(value)) {
    constructor = { name: 'MapIterator' };
    braces = ['{', '}'];
    empty = false;
    formatter = formatCollectionIterator;
  } else if (isSetIterator(value)) {
    constructor = { name: 'SetIterator' };
    braces = ['{', '}'];
    empty = false;
    formatter = formatCollectionIterator;
  } else {
    // Unset the constructor to prevent "Object {...}" for ordinary objects.
    if (constructor && constructor.name === 'Object')
      constructor = null;
    braces = ['{', '}'];
    empty = true;  // No other data than keys.
  }

  empty = empty === true && keys.length === 0;

  // Make functions say that they are functions
  if (typeof value === 'function') {
    const ctorName = constructor ? constructor.name : 'Function';
    base = ` [${ctorName}${value.name ? `: ${value.name}` : ''}]`;
  }

  // Make RegExps say that they are RegExps
  if (isRegExp(value)) {
    base = ` ${regExpToString.call(value)}`;
  }

  // Make dates with properties first say the date
  if (isDate(value)) {
    base = ` ${dateToISOString.call(value)}`;
  }

  // Make error with message first say the error
  if (isError(value)) {
    base = ` ${formatError(value)}`;
  }

  // Make boxed primitive Strings look like such
  if (typeof raw === 'string') {
    formatted = formatPrimitiveNoColor(ctx, raw);
    base = ` [String: ${formatted}]`;
  }

  // Make boxed primitive Numbers look like such
  if (typeof raw === 'number') {
    formatted = formatPrimitiveNoColor(ctx, raw);
    base = ` [Number: ${formatted}]`;
  }

  // Make boxed primitive Booleans look like such
  if (typeof raw === 'boolean') {
    formatted = formatPrimitiveNoColor(ctx, raw);
    base = ` [Boolean: ${formatted}]`;
  }

  // Add constructor name if available
  if (base === '' && constructor)
    braces[0] = `${constructor.name} ${braces[0]}`;

  if (empty === true) {
    return `${braces[0]}${base}${braces[1]}`;
  }

  if (recurseTimes < 0) {
    if (isRegExp(value)) {
      return ctx.stylize(regExpToString.call(value), 'regexp');
    } else if (Array.isArray(value)) {
      return ctx.stylize('[Array]', 'special');
    } else {
      return ctx.stylize('[Object]', 'special');
    }
  }

  ctx.seen.push(value);

  var output = formatter(ctx, value, recurseTimes, visibleKeys, keys);

  ctx.seen.pop();

  return reduceToSingleString(output, base, braces, ctx.breakLength);
}


function formatNumber(ctx, value) {
  // Format -0 as '-0'. Strict equality won't distinguish 0 from -0.
  if (Object.is(value, -0))
    return ctx.stylize('-0', 'number');
  return ctx.stylize(`${value}`, 'number');
}


function formatPrimitive(ctx, value) {
  if (value === undefined)
    return ctx.stylize('undefined', 'undefined');

  // For some reason typeof null is "object", so special case here.
  if (value === null)
    return ctx.stylize('null', 'null');

  var type = typeof value;

  if (type === 'string') {
    var simple = JSON.stringify(value)
                     .replace(/^"|"$/g, '')
                     .replace(/'/g, "\\'")
                     .replace(/\\"/g, '"');
    return ctx.stylize(`'${simple}'`, 'string');
  }
  if (type === 'number')
    return formatNumber(ctx, value);
  if (type === 'boolean')
    return ctx.stylize(`${value}`, 'boolean');
  // es6 symbol primitive
  if (type === 'symbol')
    return ctx.stylize(value.toString(), 'symbol');
}


function formatPrimitiveNoColor(ctx, value) {
  var stylize = ctx.stylize;
  ctx.stylize = stylizeNoColor;
  var str = formatPrimitive(ctx, value);
  ctx.stylize = stylize;
  return str;
}


function formatError(value) {
  return value.stack || `[${errorToString.call(value)}]`;
}


function formatObject(ctx, value, recurseTimes, visibleKeys, keys) {
  return keys.map(function(key) {
    return formatProperty(ctx, value, recurseTimes, visibleKeys, key, false);
  });
}


function formatArray(ctx, value, recurseTimes, visibleKeys, keys) {
  var output = [];
  let visibleLength = 0;
  let index = 0;
  for (const elem of keys) {
    if (visibleLength === ctx.maxArrayLength)
      break;
    // Symbols might have been added to the keys
    if (typeof elem !== 'string')
      continue;
    const i = +elem;
    if (index !== i) {
      // Skip zero and negative numbers as well as non numbers
      if (i > 0 === false)
        continue;
      const emptyItems = i - index;
      const ending = emptyItems > 1 ? 's' : '';
      const message = `<${emptyItems} empty item${ending}>`;
      output.push(ctx.stylize(message, 'undefined'));
      index = i;
      if (++visibleLength === ctx.maxArrayLength)
        break;
    }
    output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                               elem, true));
    visibleLength++;
    index++;
  }
  if (index < value.length && visibleLength !== ctx.maxArrayLength) {
    const len = value.length - index;
    const ending = len > 1 ? 's' : '';
    const message = `<${len} empty item${ending}>`;
    output.push(ctx.stylize(message, 'undefined'));
    index = value.length;
  }
  var remaining = value.length - index;
  if (remaining > 0) {
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  }
  for (var n = 0; n < keys.length; n++) {
    var key = keys[n];
    if (typeof key === 'symbol' || !numbersOnlyRE.test(key)) {
      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                                 key, true));
    }
  }
  return output;
}


function formatTypedArray(ctx, value, recurseTimes, visibleKeys, keys) {
  const maxLength = Math.min(Math.max(0, ctx.maxArrayLength), value.length);
  const remaining = value.length - maxLength;
  var output = new Array(maxLength);
  for (var i = 0; i < maxLength; ++i)
    output[i] = formatNumber(ctx, value[i]);
  if (remaining > 0) {
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  }
  for (const key of keys) {
    if (typeof key === 'symbol' || !numbersOnlyRE.test(key)) {
      output.push(
        formatProperty(ctx, value, recurseTimes, visibleKeys, key, true));
    }
  }
  return output;
}


function formatSet(ctx, value, recurseTimes, visibleKeys, keys) {
  var output = [];
  value.forEach(function(v) {
    var nextRecurseTimes = recurseTimes === null ? null : recurseTimes - 1;
    var str = formatValue(ctx, v, nextRecurseTimes);
    output.push(str);
  });
  for (var n = 0; n < keys.length; n++) {
    output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                               keys[n], false));
  }
  return output;
}


function formatMap(ctx, value, recurseTimes, visibleKeys, keys) {
  var output = [];
  value.forEach(function(v, k) {
    var nextRecurseTimes = recurseTimes === null ? null : recurseTimes - 1;
    var str = formatValue(ctx, k, nextRecurseTimes);
    str += ' => ';
    str += formatValue(ctx, v, nextRecurseTimes);
    output.push(str);
  });
  for (var n = 0; n < keys.length; n++) {
    output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                               keys[n], false));
  }
  return output;
}

function formatCollectionIterator(ctx, value, recurseTimes, visibleKeys, keys) {
  ensureDebugIsInitialized();
  const mirror = Debug.MakeMirror(value, true);
  var nextRecurseTimes = recurseTimes === null ? null : recurseTimes - 1;
  var vals = mirror.preview();
  var output = [];
  for (const o of vals) {
    output.push(formatValue(ctx, o, nextRecurseTimes));
  }
  return output;
}

function formatPromise(ctx, value, recurseTimes, visibleKeys, keys) {
  const output = [];
  const [state, result] = getPromiseDetails(value);

  if (state === kPending) {
    output.push('<pending>');
  } else {
    var nextRecurseTimes = recurseTimes === null ? null : recurseTimes - 1;
    var str = formatValue(ctx, result, nextRecurseTimes);
    if (state === kRejected) {
      output.push(`<rejected> ${str}`);
    } else {
      output.push(str);
    }
  }
  for (var n = 0; n < keys.length; n++) {
    output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                               keys[n], false));
  }
  return output;
}


function formatProperty(ctx, value, recurseTimes, visibleKeys, key, array) {
  var name, str, desc;
  desc = Object.getOwnPropertyDescriptor(value, key) || { value: value[key] };
  if (desc.get) {
    if (desc.set) {
      str = ctx.stylize('[Getter/Setter]', 'special');
    } else {
      str = ctx.stylize('[Getter]', 'special');
    }
  } else {
    if (desc.set) {
      str = ctx.stylize('[Setter]', 'special');
    }
  }
  if (visibleKeys[key] === undefined) {
    if (typeof key === 'symbol') {
      name = `[${ctx.stylize(key.toString(), 'symbol')}]`;
    } else {
      name = `[${key}]`;
    }
  }
  if (!str) {
    if (ctx.seen.indexOf(desc.value) < 0) {
      if (recurseTimes === null) {
        str = formatValue(ctx, desc.value, null);
      } else {
        str = formatValue(ctx, desc.value, recurseTimes - 1);
      }
      if (str.indexOf('\n') > -1) {
        if (array) {
          str = str.replace(/\n/g, '\n  ');
        } else {
          str = str.replace(/^|\n/g, '\n   ');
        }
      }
    } else {
      str = ctx.stylize('[Circular]', 'special');
    }
  }
  if (name === undefined) {
    if (array && numbersOnlyRE.test(key)) {
      return str;
    }
    name = JSON.stringify(`${key}`);
    if (/^"[a-zA-Z_][a-zA-Z_0-9]*"$/.test(name)) {
      name = name.substr(1, name.length - 2);
      name = ctx.stylize(name, 'name');
    } else {
      name = name.replace(/'/g, "\\'")
                 .replace(/\\"/g, '"')
                 .replace(/^"|"$/g, "'")
                 .replace(/\\\\/g, '\\');
      name = ctx.stylize(name, 'string');
    }
  }

  return `${name}: ${str}`;
}


function reduceToSingleString(output, base, braces, breakLength) {
  var length = output.reduce(function(prev, cur) {
    return prev + cur.replace(/\u001b\[\d\d?m/g, '').length + 1;
  }, 0);

  if (length > breakLength) {
    return braces[0] +
           // If the opening "brace" is too large, like in the case of "Set {",
           // we need to force the first item to be on the next line or the
           // items will not line up correctly.
           (base === '' && braces[0].length === 1 ? '' : `${base}\n `) +
           ` ${output.join(',\n  ')} ${braces[1]}`;
  }

  return `${braces[0]}${base} ${output.join(', ')} ${braces[1]}`;
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

function isRegExp(re) {
  return _isRegExp(re);
}

function isObject(arg) {
  return arg !== null && typeof arg === 'object';
}

function isDate(d) {
  return _isDate(d);
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
    throw new TypeError('The constructor to "inherits" must not be ' +
                        'null or undefined');

  if (superCtor === undefined || superCtor === null)
    throw new TypeError('The super constructor to "inherits" must not ' +
                        'be null or undefined');

  if (superCtor.prototype === undefined) {
    throw new TypeError('The super constructor to "inherits" must ' +
                        'have a prototype');
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
  var name = errname(err);
  var message = `${syscall} ${name}`;
  if (original)
    message += ` ${original}`;
  var e = new Error(message);
  e.code = name;
  e.errno = name;
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

// Avoid a circular dependency
var isBuffer;
Object.defineProperty(exports, 'isBuffer', {
  configurable: true,
  enumerable: true,
  get() {
    if (!isBuffer)
      isBuffer = require('buffer').Buffer.isBuffer;
    return isBuffer;
  },
  set(val) {
    isBuffer = val;
  }
});
