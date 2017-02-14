'use strict';

const uv = process.binding('uv');
const Buffer = require('buffer').Buffer;
const internalUtil = require('internal/util');
const binding = process.binding('util');

const isError = internalUtil.isError;

const inspectDefaultOptions = Object.seal({
  showHidden: false,
  depth: 2,
  colors: false,
  customInspect: true,
  showProxy: false,
  maxArrayLength: 100,
  breakLength: 60
});

var Debug;
var simdFormatters;

// SIMD is only available when --harmony_simd is specified on the command line
// and the set of available types differs between v5 and v6, that's why we use
// a map to look up and store the formatters.  It also provides a modicum of
// protection against users monkey-patching the SIMD object.
if (typeof global.SIMD === 'object' && global.SIMD !== null) {
  simdFormatters = new Map();

  const make =
    (extractLane, count) => (ctx, value, recurseTimes, visibleKeys, keys) => {
      const output = new Array(count);
      for (var i = 0; i < count; i += 1)
        output[i] = formatPrimitive(ctx, extractLane(value, i));
      return output;
    };

  const countPerType = {
    Bool16x8: 8,
    Bool32x4: 4,
    Bool8x16: 16,
    Float32x4: 4,
    Int16x8: 8,
    Int32x4: 4,
    Int8x16: 16,
    Uint16x8: 8,
    Uint32x4: 4,
    Uint8x16: 16
  };

  for (const key in countPerType) {
    const type = global.SIMD[key];
    simdFormatters.set(type, make(type.extractLane, countPerType[key]));
  }
}

function tryStringify(arg) {
  try {
    return JSON.stringify(arg);
  } catch (_) {
    return '[Circular]';
  }
}

exports.format = function(f) {
  if (typeof f !== 'string') {
    const objects = new Array(arguments.length);
    for (var index = 0; index < arguments.length; index++) {
      objects[index] = inspect(arguments[index]);
    }
    return objects.join(' ');
  }

  var argLen = arguments.length;

  if (argLen === 1) return f;

  var str = '';
  var a = 1;
  var lastPos = 0;
  for (var i = 0; i < f.length;) {
    if (f.charCodeAt(i) === 37/*'%'*/ && i + 1 < f.length) {
      switch (f.charCodeAt(i + 1)) {
        case 100: // 'd'
          if (a >= argLen)
            break;
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += Number(arguments[a++]);
          lastPos = i = i + 2;
          continue;
        case 106: // 'j'
          if (a >= argLen)
            break;
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += tryStringify(arguments[a++]);
          lastPos = i = i + 2;
          continue;
        case 115: // 's'
          if (a >= argLen)
            break;
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += String(arguments[a++]);
          lastPos = i = i + 2;
          continue;
        case 37: // '%'
          if (lastPos < i)
            str += f.slice(lastPos, i);
          str += '%';
          lastPos = i = i + 2;
          continue;
      }
    }
    ++i;
  }
  if (lastPos === 0)
    str = f;
  else if (lastPos < f.length)
    str += f.slice(lastPos);
  while (a < argLen) {
    const x = arguments[a++];
    if (x === null || (typeof x !== 'object' && typeof x !== 'symbol')) {
      str += ' ' + x;
    } else {
      str += ' ' + inspect(x);
    }
  }
  return str;
};


exports.deprecate = internalUtil._deprecate;


var debugs = {};
var debugEnviron;
exports.debuglog = function(set) {
  if (debugEnviron === undefined)
    debugEnviron = process.env.NODE_DEBUG || '';
  set = set.toUpperCase();
  if (!debugs[set]) {
    if (new RegExp(`\\b${set}\\b`, 'i').test(debugEnviron)) {
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
};


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
inspect.colors = {
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
};

// Don't use 'blue' not visible on cmd.exe
inspect.styles = {
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
};

const customInspectSymbol = internalUtil.customInspectSymbol;

exports.inspect = inspect;
exports.inspect.custom = customInspectSymbol;

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


function getConstructorOf(obj) {
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
}


function ensureDebugIsInitialized() {
  if (Debug === undefined) {
    const runInDebugContext = require('vm').runInDebugContext;
    Debug = runInDebugContext('Debug');
  }
}


function inspectPromise(p) {
  // Only create a mirror if the object is a Promise.
  if (!binding.isPromise(p))
    return null;
  ensureDebugIsInitialized();
  const mirror = Debug.MakeMirror(p, true);
  return {status: mirror.status(), value: mirror.promiseValue().value_};
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
      proxy = binding.getProxyDetails(value);
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
      return 'Proxy ' + formatValue(ctx, proxy, recurseTimes);
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

  if (ctx.showHidden) {
    keys = Object.getOwnPropertyNames(value);
    keys = keys.concat(Object.getOwnPropertySymbols(value));
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
      return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
    }
    if (isDate(value)) {
      if (Number.isNaN(value.getTime())) {
        return ctx.stylize(value.toString(), 'date');
      } else {
        return ctx.stylize(Date.prototype.toISOString.call(value), 'date');
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
    if (binding.isArrayBuffer(value) || binding.isSharedArrayBuffer(value)) {
      return `${constructor.name}` +
             ` { byteLength: ${formatNumber(ctx, value.byteLength)} }`;
    }
  }

  var base = '', empty = false, braces;
  var formatter = formatObject;

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
  } else if (binding.isSet(value)) {
    braces = ['{', '}'];
    // With `showHidden`, `length` will display as a hidden property for
    // arrays. For consistency's sake, do the same for `size`, even though this
    // property isn't selected by Object.getOwnPropertyNames().
    if (ctx.showHidden)
      keys.unshift('size');
    empty = value.size === 0;
    formatter = formatSet;
  } else if (binding.isMap(value)) {
    braces = ['{', '}'];
    // Ditto.
    if (ctx.showHidden)
      keys.unshift('size');
    empty = value.size === 0;
    formatter = formatMap;
  } else if (binding.isArrayBuffer(value) ||
             binding.isSharedArrayBuffer(value)) {
    braces = ['{', '}'];
    keys.unshift('byteLength');
    visibleKeys.byteLength = true;
  } else if (binding.isDataView(value)) {
    braces = ['{', '}'];
    // .buffer goes last, it's not a primitive like the others.
    keys.unshift('byteLength', 'byteOffset', 'buffer');
    visibleKeys.byteLength = true;
    visibleKeys.byteOffset = true;
    visibleKeys.buffer = true;
  } else if (binding.isTypedArray(value)) {
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
  } else {
    var promiseInternals = inspectPromise(value);
    if (promiseInternals) {
      braces = ['{', '}'];
      formatter = formatPromise;
    } else {
      let maybeSimdFormatter;
      if (binding.isMapIterator(value)) {
        constructor = { name: 'MapIterator' };
        braces = ['{', '}'];
        empty = false;
        formatter = formatCollectionIterator;
      } else if (binding.isSetIterator(value)) {
        constructor = { name: 'SetIterator' };
        braces = ['{', '}'];
        empty = false;
        formatter = formatCollectionIterator;
      } else if (simdFormatters &&
                 typeof constructor === 'function' &&
                 (maybeSimdFormatter = simdFormatters.get(constructor))) {
        braces = ['[', ']'];
        formatter = maybeSimdFormatter;
      } else {
        // Unset the constructor to prevent "Object {...}" for ordinary objects.
        if (constructor && constructor.name === 'Object')
          constructor = null;
        braces = ['{', '}'];
        empty = true;  // No other data than keys.
      }
    }
  }

  empty = empty === true && keys.length === 0;

  // Make functions say that they are functions
  if (typeof value === 'function') {
    const ctorName = constructor ? constructor.name : 'Function';
    base = ` [${ctorName}${value.name ? `: ${value.name}` : ''}]`;
  }

  // Make RegExps say that they are RegExps
  if (isRegExp(value)) {
    base = ' ' + RegExp.prototype.toString.call(value);
  }

  // Make dates with properties first say the date
  if (isDate(value)) {
    base = ' ' + Date.prototype.toISOString.call(value);
  }

  // Make error with message first say the error
  if (isError(value)) {
    base = ' ' + formatError(value);
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
    return braces[0] + base + braces[1];
  }

  if (recurseTimes < 0) {
    if (isRegExp(value)) {
      return ctx.stylize(RegExp.prototype.toString.call(value), 'regexp');
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
  // Format -0 as '-0'. Strict equality won't distinguish 0 from -0,
  // so instead we use the fact that 1 / -0 < 0 whereas 1 / 0 > 0 .
  if (value === 0 && 1 / value < 0)
    return ctx.stylize('-0', 'number');
  return ctx.stylize('' + value, 'number');
}


function formatPrimitive(ctx, value) {
  if (value === undefined)
    return ctx.stylize('undefined', 'undefined');

  // For some reason typeof null is "object", so special case here.
  if (value === null)
    return ctx.stylize('null', 'null');

  var type = typeof value;

  if (type === 'string') {
    var simple = '\'' +
                 JSON.stringify(value)
                     .replace(/^"|"$/g, '')
                     .replace(/'/g, "\\'")
                     .replace(/\\"/g, '"') +
                 '\'';
    return ctx.stylize(simple, 'string');
  }
  if (type === 'number')
    return formatNumber(ctx, value);
  if (type === 'boolean')
    return ctx.stylize('' + value, 'boolean');
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
  return value.stack || `[${Error.prototype.toString.call(value)}]`;
}


function formatObject(ctx, value, recurseTimes, visibleKeys, keys) {
  return keys.map(function(key) {
    return formatProperty(ctx, value, recurseTimes, visibleKeys, key, false);
  });
}


function formatArray(ctx, value, recurseTimes, visibleKeys, keys) {
  var output = [];
  const maxLength = Math.min(Math.max(0, ctx.maxArrayLength), value.length);
  const remaining = value.length - maxLength;
  for (var i = 0; i < maxLength; ++i) {
    if (hasOwnProperty(value, String(i))) {
      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                                 String(i), true));
    } else {
      output.push('');
    }
  }
  if (remaining > 0) {
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  }
  keys.forEach(function(key) {
    if (typeof key === 'symbol' || !key.match(/^\d+$/)) {
      output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                                 key, true));
    }
  });
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
    if (typeof key === 'symbol' || !key.match(/^\d+$/)) {
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
  keys.forEach(function(key) {
    output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                               key, false));
  });
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
  keys.forEach(function(key) {
    output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                               key, false));
  });
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
  var output = [];
  var internals = inspectPromise(value);
  if (internals.status === 'pending') {
    output.push('<pending>');
  } else {
    var nextRecurseTimes = recurseTimes === null ? null : recurseTimes - 1;
    var str = formatValue(ctx, internals.value, nextRecurseTimes);
    if (internals.status === 'rejected') {
      output.push('<rejected> ' + str);
    } else {
      output.push(str);
    }
  }
  keys.forEach(function(key) {
    output.push(formatProperty(ctx, value, recurseTimes, visibleKeys,
                               key, false));
  });
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
  if (!hasOwnProperty(visibleKeys, key)) {
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
          str = str.replace(/(^|\n)/g, '\n   ');
        }
      }
    } else {
      str = ctx.stylize('[Circular]', 'special');
    }
  }
  if (name === undefined) {
    if (array && key.match(/^\d+$/)) {
      return str;
    }
    name = JSON.stringify('' + key);
    if (name.match(/^"([a-zA-Z_][a-zA-Z_0-9]*)"$/)) {
      name = name.substr(1, name.length - 2);
      name = ctx.stylize(name, 'name');
    } else {
      name = name.replace(/'/g, "\\'")
                 .replace(/\\"/g, '"')
                 .replace(/(^"|"$)/g, "'")
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
           (base === '' && braces[0].length === 1 ? '' : base + '\n ') +
           ` ${output.join(',\n  ')} ${braces[1]}`;
  }

  return `${braces[0]}${base} ${output.join(', ')} ${braces[1]}`;
}


// NOTE: These type checking functions intentionally don't use `instanceof`
// because it is fragile and can be easily faked with `Object.create()`.
exports.isArray = Array.isArray;

function isBoolean(arg) {
  return typeof arg === 'boolean';
}
exports.isBoolean = isBoolean;

function isNull(arg) {
  return arg === null;
}
exports.isNull = isNull;

function isNullOrUndefined(arg) {
  return arg === null || arg === undefined;
}
exports.isNullOrUndefined = isNullOrUndefined;

function isNumber(arg) {
  return typeof arg === 'number';
}
exports.isNumber = isNumber;

function isString(arg) {
  return typeof arg === 'string';
}
exports.isString = isString;

function isSymbol(arg) {
  return typeof arg === 'symbol';
}
exports.isSymbol = isSymbol;

function isUndefined(arg) {
  return arg === undefined;
}
exports.isUndefined = isUndefined;

function isRegExp(re) {
  return binding.isRegExp(re);
}
exports.isRegExp = isRegExp;

function isObject(arg) {
  return arg !== null && typeof arg === 'object';
}
exports.isObject = isObject;

function isDate(d) {
  return binding.isDate(d);
}
exports.isDate = isDate;

exports.isError = isError;

function isFunction(arg) {
  return typeof arg === 'function';
}
exports.isFunction = isFunction;

function isPrimitive(arg) {
  return arg === null ||
         typeof arg !== 'object' && typeof arg !== 'function';
}
exports.isPrimitive = isPrimitive;

exports.isBuffer = Buffer.isBuffer;

function pad(n) {
  return n < 10 ? '0' + n.toString(10) : n.toString(10);
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
exports.log = function() {
  console.log('%s - %s', timestamp(), exports.format.apply(exports, arguments));
};


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
exports.inherits = function(ctor, superCtor) {

  if (ctor === undefined || ctor === null)
    throw new TypeError('The constructor to "inherits" must not be ' +
                        'null or undefined');

  if (superCtor === undefined || superCtor === null)
    throw new TypeError('The super constructor to "inherits" must not ' +
                        'be null or undefined');

  if (superCtor.prototype === undefined)
    throw new TypeError('The super constructor to "inherits" must ' +
                        'have a prototype');

  ctor.super_ = superCtor;
  Object.setPrototypeOf(ctor.prototype, superCtor.prototype);
};

exports._extend = function(target, source) {
  // Don't do anything if source isn't an object
  if (source === null || typeof source !== 'object') return target;

  var keys = Object.keys(source);
  var i = keys.length;
  while (i--) {
    target[keys[i]] = source[keys[i]];
  }
  return target;
};

function hasOwnProperty(obj, prop) {
  return Object.prototype.hasOwnProperty.call(obj, prop);
}


// Deprecated old stuff.

exports.print = internalUtil.deprecate(function() {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stdout.write(String(arguments[i]));
  }
}, 'util.print is deprecated. Use console.log instead.', 'DEP0026');


exports.puts = internalUtil.deprecate(function() {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stdout.write(arguments[i] + '\n');
  }
}, 'util.puts is deprecated. Use console.log instead.', 'DEP0027');


exports.debug = internalUtil.deprecate(function(x) {
  process.stderr.write(`DEBUG: ${x}\n`);
}, 'util.debug is deprecated. Use console.error instead.', 'DEP0028');


exports.error = internalUtil.deprecate(function(x) {
  for (var i = 0, len = arguments.length; i < len; ++i) {
    process.stderr.write(arguments[i] + '\n');
  }
}, 'util.error is deprecated. Use console.error instead.', 'DEP0029');


exports._errnoException = function(err, syscall, original) {
  var errname = uv.errname(err);
  var message = `${syscall} ${errname}`;
  if (original)
    message += ' ' + original;
  var e = new Error(message);
  e.code = errname;
  e.errno = errname;
  e.syscall = syscall;
  return e;
};


exports._exceptionWithHostPort = function(err,
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
};

// process.versions needs a custom function as some values are lazy-evaluated.
process.versions[exports.inspect.custom] =
  (depth) => exports.format(JSON.parse(JSON.stringify(process.versions)));
