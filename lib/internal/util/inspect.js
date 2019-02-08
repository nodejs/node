'use strict';

const {
  getOwnNonIndexProperties,
  getPromiseDetails,
  getProxyDetails,
  kPending,
  kRejected,
  previewEntries,
  propertyFilter: {
    ALL_PROPERTIES,
    ONLY_ENUMERABLE
  }
} = internalBinding('util');

const {
  customInspectSymbol,
  isError,
  join,
  removeColors
} = require('internal/util');

const {
  codes: {
    ERR_INVALID_ARG_TYPE
  },
  isStackOverflowError
} = require('internal/errors');

const {
  isAnyArrayBuffer,
  isArrayBuffer,
  isArgumentsObject,
  isBoxedPrimitive,
  isDataView,
  isExternal,
  isMap,
  isMapIterator,
  isModuleNamespaceObject,
  isNativeError,
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
} = require('internal/util/types');

const assert = require('internal/assert');

// Avoid monkey-patched built-ins.
const { Object } = primordials;

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
const dateToString = uncurryThis(Date.prototype.toString);
const errorToString = uncurryThis(Error.prototype.toString);

const bigIntValueOf = uncurryThis(BigInt.prototype.valueOf);
const booleanValueOf = uncurryThis(Boolean.prototype.valueOf);
const numberValueOf = uncurryThis(Number.prototype.valueOf);
const symbolValueOf = uncurryThis(Symbol.prototype.valueOf);
const stringValueOf = uncurryThis(String.prototype.valueOf);

const setValues = uncurryThis(Set.prototype.values);
const mapEntries = uncurryThis(Map.prototype.entries);
const dateGetTime = uncurryThis(Date.prototype.getTime);
const hasOwnProperty = uncurryThis(Object.prototype.hasOwnProperty);
let hexSlice;

const inspectDefaultOptions = Object.seal({
  showHidden: false,
  depth: 2,
  colors: false,
  customInspect: true,
  showProxy: false,
  maxArrayLength: 100,
  breakLength: 60,
  compact: true,
  sorted: false,
  getters: false
});

const kObjectType = 0;
const kArrayType = 1;
const kArrayExtrasType = 2;

/* eslint-disable no-control-regex */
const strEscapeSequencesRegExp = /[\x00-\x1f\x27\x5c]/;
const strEscapeSequencesReplacer = /[\x00-\x1f\x27\x5c]/g;
const strEscapeSequencesRegExpSingle = /[\x00-\x1f\x5c]/;
const strEscapeSequencesReplacerSingle = /[\x00-\x1f\x5c]/g;
/* eslint-enable no-control-regex */

const keyStrRegExp = /^[a-zA-Z_][a-zA-Z_0-9]*$/;
const numberRegExp = /^(0|[1-9][0-9]*)$/;

const readableRegExps = {};

const kMinLineLength = 16;

// Constants to map the iterator state.
const kWeak = 0;
const kIterator = 1;
const kMapEntries = 2;

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
    budget: {},
    indentationLvl: 0,
    seen: [],
    stylize: stylizeNoColor,
    showHidden: inspectDefaultOptions.showHidden,
    depth: inspectDefaultOptions.depth,
    colors: inspectDefaultOptions.colors,
    customInspect: inspectDefaultOptions.customInspect,
    showProxy: inspectDefaultOptions.showProxy,
    maxArrayLength: inspectDefaultOptions.maxArrayLength,
    breakLength: inspectDefaultOptions.breakLength,
    compact: inspectDefaultOptions.compact,
    sorted: inspectDefaultOptions.sorted,
    getters: inspectDefaultOptions.getters
  };
  if (arguments.length > 1) {
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
  }
  if (ctx.colors) ctx.stylize = stylizeWithColor;
  if (ctx.maxArrayLength === null) ctx.maxArrayLength = Infinity;
  return formatValue(ctx, value, 0);
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
    return Object.assign(inspectDefaultOptions, options);
  }
});

// http://en.wikipedia.org/wiki/ANSI_escape_code#graphics
inspect.colors = Object.assign(Object.create(null), {
  bold: [1, 22],
  italic: [3, 23],
  underline: [4, 24],
  inverse: [7, 27],
  white: [37, 39],
  grey: [90, 39],
  black: [30, 39],
  blue: [34, 39],
  cyan: [36, 39],
  green: [32, 39],
  magenta: [35, 39],
  red: [31, 39],
  yellow: [33, 39]
});

// Don't use 'blue' not visible on cmd.exe
inspect.styles = Object.assign(Object.create(null), {
  special: 'cyan',
  number: 'yellow',
  bigint: 'yellow',
  boolean: 'yellow',
  undefined: 'grey',
  null: 'bold',
  string: 'green',
  symbol: 'green',
  date: 'magenta',
  // "name": intentionally not styling
  regexp: 'red'
});

function addQuotes(str, quotes) {
  if (quotes === -1) {
    return `"${str}"`;
  }
  if (quotes === -2) {
    return `\`${str}\``;
  }
  return `'${str}'`;
}

const escapeFn = (str) => meta[str.charCodeAt(0)];

// Escape control characters, single quotes and the backslash.
// This is similar to JSON stringify escaping.
function strEscape(str) {
  let escapeTest = strEscapeSequencesRegExp;
  let escapeReplace = strEscapeSequencesReplacer;
  let singleQuote = 39;

  // Check for double quotes. If not present, do not escape single quotes and
  // instead wrap the text in double quotes. If double quotes exist, check for
  // backticks. If they do not exist, use those as fallback instead of the
  // double quotes.
  if (str.indexOf("'") !== -1) {
    // This invalidates the charCode and therefore can not be matched for
    // anymore.
    if (str.indexOf('"') === -1) {
      singleQuote = -1;
    } else if (str.indexOf('`') === -1 && str.indexOf('${') === -1) {
      singleQuote = -2;
    }
    if (singleQuote !== 39) {
      escapeTest = strEscapeSequencesRegExpSingle;
      escapeReplace = strEscapeSequencesReplacerSingle;
    }
  }

  // Some magic numbers that worked out fine while benchmarking with v8 6.0
  if (str.length < 5000 && !escapeTest.test(str))
    return addQuotes(str, singleQuote);
  if (str.length > 100) {
    str = str.replace(escapeReplace, escapeFn);
    return addQuotes(str, singleQuote);
  }

  let result = '';
  let last = 0;
  for (var i = 0; i < str.length; i++) {
    const point = str.charCodeAt(i);
    if (point === singleQuote || point === 92 || point < 32) {
      if (last === i) {
        result += meta[point];
      } else {
        result += `${str.slice(last, i)}${meta[point]}`;
      }
      last = i + 1;
    }
  }

  if (last !== i) {
    result += str.slice(last);
  }
  return addQuotes(result, singleQuote);
}

function stylizeWithColor(str, styleType) {
  const style = inspect.styles[styleType];
  if (style !== undefined) {
    const color = inspect.colors[style];
    return `\u001b[${color[0]}m${str}\u001b[${color[1]}m`;
  }
  return str;
}

function stylizeNoColor(str) {
  return str;
}

// Return a new empty array to push in the results of the default formatter.
function getEmptyFormatArray() {
  return [];
}

function getConstructorName(obj, ctx) {
  let firstProto;
  while (obj) {
    const descriptor = Object.getOwnPropertyDescriptor(obj, 'constructor');
    if (descriptor !== undefined &&
        typeof descriptor.value === 'function' &&
        descriptor.value.name !== '') {
      return descriptor.value.name;
    }

    obj = Object.getPrototypeOf(obj);
    if (firstProto === undefined) {
      firstProto = obj;
    }
  }

  if (firstProto === null) {
    return null;
  }

  return `<${inspect(firstProto, {
    ...ctx,
    customInspect: false
  })}>`;
}

function getPrefix(constructor, tag, fallback) {
  if (constructor === null) {
    if (tag !== '') {
      return `[${fallback}: null prototype] [${tag}] `;
    }
    return `[${fallback}: null prototype] `;
  }

  if (tag !== '' && constructor !== tag) {
    return `${constructor} [${tag}] `;
  }
  return `${constructor} `;
}

const getBoxedValue = formatPrimitive.bind(null, stylizeNoColor);

// Look up the keys of the object.
function getKeys(value, showHidden) {
  let keys;
  const symbols = Object.getOwnPropertySymbols(value);
  if (showHidden) {
    keys = Object.getOwnPropertyNames(value);
    if (symbols.length !== 0)
      keys.push(...symbols);
  } else {
    // This might throw if `value` is a Module Namespace Object from an
    // unevaluated module, but we don't want to perform the actual type
    // check because it's expensive.
    // TODO(devsnek): track https://github.com/tc39/ecma262/issues/1209
    // and modify this logic as needed.
    try {
      keys = Object.keys(value);
    } catch (err) {
      assert(isNativeError(err) && err.name === 'ReferenceError' &&
             isModuleNamespaceObject(value));
      keys = Object.getOwnPropertyNames(value);
    }
    if (symbols.length !== 0) {
      keys.push(...symbols.filter((key) => propertyIsEnumerable(value, key)));
    }
  }
  return keys;
}

function getCtxStyle(constructor, tag) {
  return constructor || tag || 'Object';
}

function formatProxy(ctx, proxy, recurseTimes) {
  if (recurseTimes > ctx.depth && ctx.depth !== null) {
    return ctx.stylize('Proxy [Array]', 'special');
  }
  recurseTimes += 1;
  ctx.indentationLvl += 2;
  const res = [
    formatValue(ctx, proxy[0], recurseTimes),
    formatValue(ctx, proxy[1], recurseTimes)
  ];
  ctx.indentationLvl -= 2;
  const str = reduceToSingleString(ctx, res, '', ['[', ']']);
  return `Proxy ${str}`;
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

let lazyNullPrototypeCache;
// Creates a subclass and name
// the constructor as `${clazz} : null prototype`
function clazzWithNullPrototype(clazz, name) {
  if (lazyNullPrototypeCache === undefined) {
    lazyNullPrototypeCache = new Map();
  } else {
    const cachedClass = lazyNullPrototypeCache.get(clazz);
    if (cachedClass !== undefined) {
      return cachedClass;
    }
  }
  class NullPrototype extends clazz {
    get [Symbol.toStringTag]() {
      return '';
    }
  }
  Object.defineProperty(NullPrototype.prototype.constructor, 'name',
                        { value: `[${name}: null prototype]` });
  lazyNullPrototypeCache.set(clazz, NullPrototype);
  return NullPrototype;
}

function noPrototypeIterator(ctx, value, recurseTimes) {
  let newVal;
  if (isSet(value)) {
    const clazz = clazzWithNullPrototype(Set, 'Set');
    newVal = new clazz(setValues(value));
  } else if (isMap(value)) {
    const clazz = clazzWithNullPrototype(Map, 'Map');
    newVal = new clazz(mapEntries(value));
  } else if (Array.isArray(value)) {
    const clazz = clazzWithNullPrototype(Array, 'Array');
    newVal = new clazz(value.length);
  } else if (isTypedArray(value)) {
    const constructor = findTypedConstructor(value);
    const clazz = clazzWithNullPrototype(constructor, constructor.name);
    newVal = new clazz(value);
  }
  if (newVal !== undefined) {
    Object.defineProperties(newVal, Object.getOwnPropertyDescriptors(value));
    return formatRaw(ctx, newVal, recurseTimes);
  }
}

// Note: using `formatValue` directly requires the indentation level to be
// corrected by setting `ctx.indentationLvL += diff` and then to decrease the
// value afterwards again.
function formatValue(ctx, value, recurseTimes, typedArray) {
  // Primitive types cannot have properties.
  if (typeof value !== 'object' && typeof value !== 'function') {
    return formatPrimitive(ctx.stylize, value, ctx);
  }
  if (value === null) {
    return ctx.stylize('null', 'null');
  }

  if (ctx.stop !== undefined) {
    const name = getConstructorName(value, ctx) || value[Symbol.toStringTag];
    return ctx.stylize(`[${name || 'Object'}]`, 'special');
  }

  if (ctx.showProxy) {
    const proxy = getProxyDetails(value);
    if (proxy !== undefined) {
      return formatProxy(ctx, proxy, recurseTimes);
    }
  }

  // Provide a hook for user-specified inspect functions.
  // Check that value is an object with an inspect function on it.
  if (ctx.customInspect) {
    const maybeCustom = value[customInspectSymbol];
    if (typeof maybeCustom === 'function' &&
        // Filter out the util module, its inspect function is special.
        maybeCustom !== inspect &&
        // Also filter out any prototype objects using the circular check.
        !(value.constructor && value.constructor.prototype === value)) {
      // This makes sure the recurseTimes are reported as before while using
      // a counter internally.
      const depth = ctx.depth === null ? null : ctx.depth - recurseTimes;
      const ret = maybeCustom.call(value, depth, ctx);

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

  return formatRaw(ctx, value, recurseTimes, typedArray);
}

function formatRaw(ctx, value, recurseTimes, typedArray) {
  let keys;

  const constructor = getConstructorName(value, ctx);
  let tag = value[Symbol.toStringTag];
  if (typeof tag !== 'string')
    tag = '';
  let base = '';
  let formatter = getEmptyFormatArray;
  let braces;
  let noIterator = true;
  let i = 0;
  const filter = ctx.showHidden ? ALL_PROPERTIES : ONLY_ENUMERABLE;

  let extrasType = kObjectType;

  // Iterators and the rest are split to reduce checks.
  if (value[Symbol.iterator]) {
    noIterator = false;
    if (Array.isArray(value)) {
      keys = getOwnNonIndexProperties(value, filter);
      // Only set the constructor for non ordinary ("Array [...]") arrays.
      const prefix = getPrefix(constructor, tag, 'Array');
      braces = [`${prefix === 'Array ' ? '' : prefix}[`, ']'];
      if (value.length === 0 && keys.length === 0)
        return `${braces[0]}]`;
      extrasType = kArrayExtrasType;
      formatter = formatArray;
    } else if (isSet(value)) {
      keys = getKeys(value, ctx.showHidden);
      const prefix = getPrefix(constructor, tag, 'Set');
      if (value.size === 0 && keys.length === 0)
        return `${prefix}{}`;
      braces = [`${prefix}{`, '}'];
      formatter = formatSet;
    } else if (isMap(value)) {
      keys = getKeys(value, ctx.showHidden);
      const prefix = getPrefix(constructor, tag, 'Map');
      if (value.size === 0 && keys.length === 0)
        return `${prefix}{}`;
      braces = [`${prefix}{`, '}'];
      formatter = formatMap;
    } else if (isTypedArray(value)) {
      keys = getOwnNonIndexProperties(value, filter);
      const prefix = constructor !== null ?
        getPrefix(constructor, tag) :
        getPrefix(constructor, tag, findTypedConstructor(value).name);
      braces = [`${prefix}[`, ']'];
      if (value.length === 0 && keys.length === 0 && !ctx.showHidden)
        return `${braces[0]}]`;
      formatter = formatTypedArray;
      extrasType = kArrayExtrasType;
    } else if (isMapIterator(value)) {
      keys = getKeys(value, ctx.showHidden);
      braces = [`[${tag}] {`, '}'];
      formatter = formatMapIterator;
    } else if (isSetIterator(value)) {
      keys = getKeys(value, ctx.showHidden);
      braces = [`[${tag}] {`, '}'];
      formatter = formatSetIterator;
    } else {
      noIterator = true;
    }
  }
  if (noIterator) {
    keys = getKeys(value, ctx.showHidden);
    braces = ['{', '}'];
    if (constructor === 'Object') {
      if (isArgumentsObject(value)) {
        braces[0] = '[Arguments] {';
      } else if (tag !== '') {
        braces[0] = `${getPrefix(constructor, tag, 'Object')}{`;
      }
      if (keys.length === 0) {
        return `${braces[0]}}`;
      }
    } else if (typeof value === 'function') {
      const type = constructor || tag || 'Function';
      let name = `${type}`;
      if (value.name && typeof value.name === 'string') {
        name += `: ${value.name}`;
      }
      if (keys.length === 0)
        return ctx.stylize(`[${name}]`, 'special');
      base = `[${name}]`;
    } else if (isRegExp(value)) {
      // Make RegExps say that they are RegExps
      base = regExpToString(constructor !== null ? value : new RegExp(value));
      const prefix = getPrefix(constructor, tag, 'RegExp');
      if (prefix !== 'RegExp ')
        base = `${prefix}${base}`;
      if (keys.length === 0 || recurseTimes > ctx.depth && ctx.depth !== null)
        return ctx.stylize(base, 'regexp');
    } else if (isDate(value)) {
      // Make dates with properties first say the date
      base = Number.isNaN(dateGetTime(value)) ?
        dateToString(value) :
        dateToISOString(value);
      const prefix = getPrefix(constructor, tag, 'Date');
      if (prefix !== 'Date ')
        base = `${prefix}${base}`;
      if (keys.length === 0) {
        return ctx.stylize(base, 'date');
      }
    } else if (isError(value)) {
      // Make error with message first say the error.
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
      if (keys.length === 0)
        return base;

      if (ctx.compact === false && stackStart !== -1) {
        braces[0] += `${base.slice(stackStart)}`;
        base = `[${base.slice(0, stackStart)}]`;
      }
    } else if (isAnyArrayBuffer(value)) {
      // Fast path for ArrayBuffer and SharedArrayBuffer.
      // Can't do the same for DataView because it has a non-primitive
      // .buffer property that we need to recurse for.
      const arrayType = isArrayBuffer(value) ? 'ArrayBuffer' :
        'SharedArrayBuffer';
      const prefix = getPrefix(constructor, tag, arrayType);
      if (typedArray === undefined) {
        formatter = formatArrayBuffer;
      } else if (keys.length === 0) {
        return prefix +
              `{ byteLength: ${formatNumber(ctx.stylize, value.byteLength)} }`;
      }
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
      formatter = ctx.showHidden ? formatWeakSet : formatWeakCollection;
    } else if (isWeakMap(value)) {
      braces[0] = `${getPrefix(constructor, tag, 'WeakMap')}{`;
      formatter = ctx.showHidden ? formatWeakMap : formatWeakCollection;
    } else if (isModuleNamespaceObject(value)) {
      braces[0] = `[${tag}] {`;
      formatter = formatNamespaceObject;
    } else if (isBoxedPrimitive(value)) {
      let type;
      if (isNumberObject(value)) {
        base = `[Number: ${getBoxedValue(numberValueOf(value))}]`;
        type = 'number';
      } else if (isStringObject(value)) {
        base = `[String: ${getBoxedValue(stringValueOf(value), ctx)}]`;
        type = 'string';
        // For boxed Strings, we have to remove the 0-n indexed entries,
        // since they just noisy up the output and are redundant
        // Make boxed primitive Strings look like such
        keys = keys.slice(value.length);
      } else if (isBooleanObject(value)) {
        base = `[Boolean: ${getBoxedValue(booleanValueOf(value))}]`;
        type = 'boolean';
      } else if (isBigIntObject(value)) {
        base = `[BigInt: ${getBoxedValue(bigIntValueOf(value))}]`;
        type = 'bigint';
      } else {
        base = `[Symbol: ${getBoxedValue(symbolValueOf(value))}]`;
        type = 'symbol';
      }
      if (keys.length === 0) {
        return ctx.stylize(base, type);
      }
    } else {
      // The input prototype got manipulated. Special handle these. We have to
      // rebuild the information so we are able to display everything.
      if (constructor === null) {
        const specialIterator = noPrototypeIterator(ctx, value, recurseTimes);
        if (specialIterator) {
          return specialIterator;
        }
      }
      if (isMapIterator(value)) {
        braces = [`[${tag || 'Map Iterator'}] {`, '}'];
        formatter = formatMapIterator;
      } else if (isSetIterator(value)) {
        braces = [`[${tag || 'Set Iterator'}] {`, '}'];
        formatter = formatSetIterator;
      // Handle other regular objects again.
      } else if (keys.length === 0) {
        if (isExternal(value))
          return ctx.stylize('[External]', 'special');
        return `${getPrefix(constructor, tag, 'Object')}{}`;
      } else {
        braces[0] = `${getPrefix(constructor, tag, 'Object')}{`;
      }
    }
  }

  if (recurseTimes > ctx.depth && ctx.depth !== null) {
    return ctx.stylize(`[${getCtxStyle(constructor, tag)}]`, 'special');
  }
  recurseTimes += 1;

  ctx.seen.push(value);
  let output;
  const indentationLvl = ctx.indentationLvl;
  try {
    output = formatter(ctx, value, recurseTimes, keys);
    for (i = 0; i < keys.length; i++) {
      output.push(
        formatProperty(ctx, value, recurseTimes, keys[i], extrasType));
    }
  } catch (err) {
    return handleMaxCallStackSize(ctx, err, constructor, tag, indentationLvl);
  }
  ctx.seen.pop();

  if (ctx.sorted) {
    const comparator = ctx.sorted === true ? undefined : ctx.sorted;
    if (extrasType === kObjectType) {
      output = output.sort(comparator);
    } else if (keys.length > 1) {
      const sorted = output.slice(output.length - keys.length).sort(comparator);
      output.splice(output.length - keys.length, keys.length, ...sorted);
    }
  }

  const res = reduceToSingleString(ctx, output, base, braces);
  const budget = ctx.budget[ctx.indentationLvl] || 0;
  const newLength = budget + res.length;
  ctx.budget[ctx.indentationLvl] = newLength;
  // If any indentationLvl exceeds this limit, limit further inspecting to the
  // minimum. Otherwise the recursive algorithm might continue inspecting the
  // object even though the maximum string size (~2 ** 28 on 32 bit systems and
  // ~2 ** 30 on 64 bit systems) exceeded. The actual output is not limited at
  // exactly 2 ** 27 but a bit higher. This depends on the object shape.
  // This limit also makes sure that huge objects don't block the event loop
  // significantly.
  if (newLength > 2 ** 27) {
    ctx.stop = true;
  }
  return res;
}

function handleMaxCallStackSize(ctx, err, constructor, tag, indentationLvl) {
  if (isStackOverflowError(err)) {
    ctx.seen.pop();
    ctx.indentationLvl = indentationLvl;
    return ctx.stylize(
      `[${getCtxStyle(constructor, tag)}: Inspection interrupted ` +
        'prematurely. Maximum call stack size exceeded.]',
      'special'
    );
  }
  throw err;
}

function formatNumber(fn, value) {
  // Format -0 as '-0'. Checking `value === -0` won't distinguish 0 from -0.
  return fn(Object.is(value, -0) ? '-0' : `${value}`, 'number');
}

function formatBigInt(fn, value) {
  return fn(`${value}n`, 'bigint');
}

function formatPrimitive(fn, value, ctx) {
  if (typeof value === 'string') {
    if (ctx.compact === false &&
      ctx.indentationLvl + value.length > ctx.breakLength &&
      value.length > kMinLineLength) {
      const rawMaxLineLength = ctx.breakLength - ctx.indentationLvl;
      const maxLineLength = Math.max(rawMaxLineLength, kMinLineLength);
      const lines = Math.ceil(value.length / maxLineLength);
      const averageLineLength = Math.ceil(value.length / lines);
      const divisor = Math.max(averageLineLength, kMinLineLength);
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
        let res = `${fn(strEscape(matches[0]), 'string')} +\n`;
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
    return formatBigInt(fn, value);
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

function formatNamespaceObject(ctx, value, recurseTimes, keys) {
  const output = new Array(keys.length);
  for (var i = 0; i < keys.length; i++) {
    try {
      output[i] = formatProperty(ctx, value, recurseTimes, keys[i],
                                 kObjectType);
    } catch (err) {
      if (!(isNativeError(err) && err.name === 'ReferenceError')) {
        throw err;
      }
      // Use the existing functionality. This makes sure the indentation and
      // line breaks are always correct. Otherwise it is very difficult to keep
      // this aligned, even though this is a hacky way of dealing with this.
      const tmp = { [keys[i]]: '' };
      output[i] = formatProperty(ctx, tmp, recurseTimes, keys[i], kObjectType);
      const pos = output[i].lastIndexOf(' ');
      // We have to find the last whitespace and have to replace that value as
      // it will be visualized as a regular string.
      output[i] = output[i].slice(0, pos + 1) +
                  ctx.stylize('<uninitialized>', 'special');
    }
  }
  // Reset the keys to an empty array. This prevents duplicated inspection.
  keys.length = 0;
  return output;
}

// The array is sparse and/or has extra keys
function formatSpecialArray(ctx, value, recurseTimes, maxLength, output, i) {
  const keys = Object.keys(value);
  let index = i;
  for (; i < keys.length && output.length < maxLength; i++) {
    const key = keys[i];
    const tmp = +key;
    // Arrays can only have up to 2^32 - 1 entries
    if (tmp > 2 ** 32 - 2) {
      break;
    }
    if (`${index}` !== key) {
      if (!numberRegExp.test(key)) {
        break;
      }
      const emptyItems = tmp - index;
      const ending = emptyItems > 1 ? 's' : '';
      const message = `<${emptyItems} empty item${ending}>`;
      output.push(ctx.stylize(message, 'undefined'));
      index = tmp;
      if (output.length === maxLength) {
        break;
      }
    }
    output.push(formatProperty(ctx, value, recurseTimes, key, kArrayType));
    index++;
  }
  const remaining = value.length - index;
  if (output.length !== maxLength) {
    if (remaining > 0) {
      const ending = remaining > 1 ? 's' : '';
      const message = `<${remaining} empty item${ending}>`;
      output.push(ctx.stylize(message, 'undefined'));
    }
  } else if (remaining > 0) {
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  }
  return output;
}

function formatArrayBuffer(ctx, value) {
  const buffer = new Uint8Array(value);
  if (hexSlice === undefined)
    hexSlice = uncurryThis(require('buffer').Buffer.prototype.hexSlice);
  let str = hexSlice(buffer, 0, Math.min(ctx.maxArrayLength, buffer.length))
    .replace(/(.{2})/g, '$1 ').trim();
  const remaining = buffer.length - ctx.maxArrayLength;
  if (remaining > 0)
    str += ` ... ${remaining} more byte${remaining > 1 ? 's' : ''}`;
  return [`${ctx.stylize('[Uint8Contents]', 'special')}: <${str}>`];
}

function formatArray(ctx, value, recurseTimes) {
  const valLen = value.length;
  const len = Math.min(Math.max(0, ctx.maxArrayLength), valLen);

  const remaining = valLen - len;
  const output = [];
  for (var i = 0; i < len; i++) {
    // Special handle sparse arrays.
    if (!hasOwnProperty(value, i)) {
      return formatSpecialArray(ctx, value, recurseTimes, len, output, i);
    }
    output.push(formatProperty(ctx, value, recurseTimes, i, kArrayType));
  }
  if (remaining > 0)
    output.push(`... ${remaining} more item${remaining > 1 ? 's' : ''}`);
  return output;
}

function formatTypedArray(ctx, value, recurseTimes) {
  const maxLength = Math.min(Math.max(0, ctx.maxArrayLength), value.length);
  const remaining = value.length - maxLength;
  const output = new Array(maxLength);
  const elementFormatter = value.length > 0 && typeof value[0] === 'number' ?
    formatNumber :
    formatBigInt;
  for (var i = 0; i < maxLength; ++i)
    output[i] = elementFormatter(ctx.stylize, value[i]);
  if (remaining > 0) {
    output[i] = `... ${remaining} more item${remaining > 1 ? 's' : ''}`;
  }
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
      const str = formatValue(ctx, value[key], recurseTimes, true);
      output.push(`[${key}]: ${str}`);
    }
    ctx.indentationLvl -= 2;
  }
  return output;
}

function formatSet(ctx, value, recurseTimes) {
  const output = [];
  ctx.indentationLvl += 2;
  for (const v of value) {
    output.push(formatValue(ctx, v, recurseTimes));
  }
  ctx.indentationLvl -= 2;
  // With `showHidden`, `length` will display as a hidden property for
  // arrays. For consistency's sake, do the same for `size`, even though this
  // property isn't selected by Object.getOwnPropertyNames().
  if (ctx.showHidden)
    output.push(`[size]: ${ctx.stylize(`${value.size}`, 'number')}`);
  return output;
}

function formatMap(ctx, value, recurseTimes) {
  const output = [];
  ctx.indentationLvl += 2;
  for (const [k, v] of value) {
    output.push(`${formatValue(ctx, k, recurseTimes)} => ` +
                formatValue(ctx, v, recurseTimes));
  }
  ctx.indentationLvl -= 2;
  // See comment in formatSet
  if (ctx.showHidden)
    output.push(`[size]: ${ctx.stylize(`${value.size}`, 'number')}`);
  return output;
}

function formatSetIterInner(ctx, recurseTimes, entries, state) {
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
  return output;
}

function formatMapIterInner(ctx, recurseTimes, entries, state) {
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
  return output;
}

function formatWeakCollection(ctx) {
  return [ctx.stylize('<items unknown>', 'special')];
}

function formatWeakSet(ctx, value, recurseTimes) {
  const entries = previewEntries(value);
  return formatSetIterInner(ctx, recurseTimes, entries, kWeak);
}

function formatWeakMap(ctx, value, recurseTimes) {
  const entries = previewEntries(value);
  return formatMapIterInner(ctx, recurseTimes, entries, kWeak);
}

function formatSetIterator(ctx, value, recurseTimes) {
  const entries = previewEntries(value);
  return formatSetIterInner(ctx, recurseTimes, entries, kIterator);
}

function formatMapIterator(ctx, value, recurseTimes) {
  const [entries, isKeyValue] = previewEntries(value, true);
  if (isKeyValue) {
    return formatMapIterInner(ctx, recurseTimes, entries, kMapEntries);
  }

  return formatSetIterInner(ctx, recurseTimes, entries, kIterator);
}

function formatPromise(ctx, value, recurseTimes) {
  let output;
  const [state, result] = getPromiseDetails(value);
  if (state === kPending) {
    output = [ctx.stylize('<pending>', 'special')];
  } else {
    ctx.indentationLvl += 2;
    const str = formatValue(ctx, result, recurseTimes);
    ctx.indentationLvl -= 2;
    output = [
      state === kRejected ?
        `${ctx.stylize('<rejected>', 'special')} ${str}` :
        str
    ];
  }
  return output;
}

function formatProperty(ctx, value, recurseTimes, key, type) {
  let name, str;
  let extra = ' ';
  const desc = Object.getOwnPropertyDescriptor(value, key) ||
    { value: value[key], enumerable: true };
  if (desc.value !== undefined) {
    const diff = (type !== kObjectType || ctx.compact === false) ? 2 : 3;
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
    const label = desc.set !== undefined ? 'Getter/Setter' : 'Getter';
    const s = ctx.stylize;
    const sp = 'special';
    if (ctx.getters && (ctx.getters === true ||
          ctx.getters === 'get' && desc.set === undefined ||
          ctx.getters === 'set' && desc.set !== undefined)) {
      try {
        const tmp = value[key];
        ctx.indentationLvl += 2;
        if (tmp === null) {
          str = `${s(`[${label}:`, sp)} ${s('null', 'null')}${s(']', sp)}`;
        } else if (typeof tmp === 'object') {
          str = `${s(`[${label}]`, sp)} ${formatValue(ctx, tmp, recurseTimes)}`;
        } else {
          const primitive = formatPrimitive(s, tmp, ctx);
          str = `${s(`[${label}:`, sp)} ${primitive}${s(']', sp)}`;
        }
        ctx.indentationLvl -= 2;
      } catch (err) {
        const message = `<Inspection threw (${err.message})>`;
        str = `${s(`[${label}:`, sp)} ${message}${s(']', sp)}`;
      }
    } else {
      str = ctx.stylize(`[${label}]`, sp);
    }
  } else if (desc.set !== undefined) {
    str = ctx.stylize('[Setter]', 'special');
  } else {
    str = ctx.stylize('undefined', 'undefined');
  }
  if (type === kArrayType) {
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

module.exports = {
  inspect,
  formatProperty,
  kObjectType
};
