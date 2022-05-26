'use strict';

const {
  ArrayFrom,
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  Error,
  ObjectCreate,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetOwnPropertyDescriptors,
  ObjectGetPrototypeOf,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  Promise,
  ReflectApply,
  ReflectConstruct,
  RegExpPrototypeExec,
  RegExpPrototypeTest,
  SafeMap,
  SafeSet,
  StringPrototypeReplace,
  StringPrototypeToLowerCase,
  StringPrototypeToUpperCase,
  Symbol,
  SymbolFor,
} = primordials;

const {
  hideStackFrames,
  codes: {
    ERR_NO_CRYPTO,
    ERR_UNKNOWN_SIGNAL
  },
  uvErrmapGet,
  overrideStackTrace,
} = require('internal/errors');
const { signals } = internalBinding('constants').os;
const {
  getHiddenValue,
  setHiddenValue,
  arrow_message_private_symbol: kArrowMessagePrivateSymbolIndex,
  decorated_private_symbol: kDecoratedPrivateSymbolIndex,
  sleep: _sleep,
  toUSVString: _toUSVString,
} = internalBinding('util');
const { isNativeError } = internalBinding('types');

const noCrypto = !process.versions.openssl;

const experimentalWarnings = new SafeSet();

const colorRegExp = /\u001b\[\d\d?m/g; // eslint-disable-line no-control-regex

const unpairedSurrogateRe =
  /(?:[^\uD800-\uDBFF]|^)[\uDC00-\uDFFF]|[\uD800-\uDBFF](?![\uDC00-\uDFFF])/;
function toUSVString(val) {
  const str = `${val}`;
  // As of V8 5.5, `str.search()` (and `unpairedSurrogateRe[@@search]()`) are
  // slower than `unpairedSurrogateRe.exec()`.
  const match = RegExpPrototypeExec(unpairedSurrogateRe, str);
  if (!match)
    return str;
  return _toUSVString(str, match.index);
}

let uvBinding;

function lazyUv() {
  uvBinding ??= internalBinding('uv');
  return uvBinding;
}

function removeColors(str) {
  return StringPrototypeReplace(str, colorRegExp, '');
}

function isError(e) {
  // An error could be an instance of Error while not being a native error
  // or could be from a different realm and not be instance of Error but still
  // be a native error.
  return isNativeError(e) || e instanceof Error;
}

// Keep a list of deprecation codes that have been warned on so we only warn on
// each one once.
const codesWarned = new SafeSet();

let validateString;

// Mark that a method should not be used.
// Returns a modified function which warns once by default.
// If --no-deprecation is set, then it is a no-op.
function deprecate(fn, msg, code) {
  if (process.noDeprecation === true) {
    return fn;
  }

  // Lazy-load to avoid a circular dependency.
  if (validateString === undefined)
    ({ validateString } = require('internal/validators'));

  if (code !== undefined)
    validateString(code, 'code');

  let warned = false;
  function deprecated(...args) {
    if (!warned) {
      warned = true;
      if (code !== undefined) {
        if (!codesWarned.has(code)) {
          process.emitWarning(msg, 'DeprecationWarning', code, deprecated);
          codesWarned.add(code);
        }
      } else {
        process.emitWarning(msg, 'DeprecationWarning', deprecated);
      }
    }
    if (new.target) {
      return ReflectConstruct(fn, args, new.target);
    }
    return ReflectApply(fn, this, args);
  }

  // The wrapper will keep the same prototype as fn to maintain prototype chain
  ObjectSetPrototypeOf(deprecated, fn);
  if (fn.prototype) {
    // Setting this (rather than using Object.setPrototype, as above) ensures
    // that calling the unwrapped constructor gives an instanceof the wrapped
    // constructor.
    deprecated.prototype = fn.prototype;
  }

  return deprecated;
}

function decorateErrorStack(err) {
  if (!(isError(err) && err.stack) ||
      getHiddenValue(err, kDecoratedPrivateSymbolIndex) === true)
    return;

  const arrow = getHiddenValue(err, kArrowMessagePrivateSymbolIndex);

  if (arrow) {
    err.stack = arrow + err.stack;
    setHiddenValue(err, kDecoratedPrivateSymbolIndex, true);
  }
}

function assertCrypto() {
  if (noCrypto)
    throw new ERR_NO_CRYPTO();
}

// Return undefined if there is no match.
// Move the "slow cases" to a separate function to make sure this function gets
// inlined properly. That prioritizes the common case.
function normalizeEncoding(enc) {
  if (enc == null || enc === 'utf8' || enc === 'utf-8') return 'utf8';
  return slowCases(enc);
}

function slowCases(enc) {
  switch (enc.length) {
    case 4:
      if (enc === 'UTF8') return 'utf8';
      if (enc === 'ucs2' || enc === 'UCS2') return 'utf16le';
      enc = `${enc}`.toLowerCase();
      if (enc === 'utf8') return 'utf8';
      if (enc === 'ucs2') return 'utf16le';
      break;
    case 3:
      if (enc === 'hex' || enc === 'HEX' ||
          `${enc}`.toLowerCase() === 'hex')
        return 'hex';
      break;
    case 5:
      if (enc === 'ascii') return 'ascii';
      if (enc === 'ucs-2') return 'utf16le';
      if (enc === 'UTF-8') return 'utf8';
      if (enc === 'ASCII') return 'ascii';
      if (enc === 'UCS-2') return 'utf16le';
      enc = `${enc}`.toLowerCase();
      if (enc === 'utf-8') return 'utf8';
      if (enc === 'ascii') return 'ascii';
      if (enc === 'ucs-2') return 'utf16le';
      break;
    case 6:
      if (enc === 'base64') return 'base64';
      if (enc === 'latin1' || enc === 'binary') return 'latin1';
      if (enc === 'BASE64') return 'base64';
      if (enc === 'LATIN1' || enc === 'BINARY') return 'latin1';
      enc = `${enc}`.toLowerCase();
      if (enc === 'base64') return 'base64';
      if (enc === 'latin1' || enc === 'binary') return 'latin1';
      break;
    case 7:
      if (enc === 'utf16le' || enc === 'UTF16LE' ||
          `${enc}`.toLowerCase() === 'utf16le')
        return 'utf16le';
      break;
    case 8:
      if (enc === 'utf-16le' || enc === 'UTF-16LE' ||
        `${enc}`.toLowerCase() === 'utf-16le')
        return 'utf16le';
      break;
    case 9:
      if (enc === 'base64url' || enc === 'BASE64URL' ||
          `${enc}`.toLowerCase() === 'base64url')
        return 'base64url';
      break;
    default:
      if (enc === '') return 'utf8';
  }
}

function emitExperimentalWarning(feature) {
  if (experimentalWarnings.has(feature)) return;
  const msg = `${feature} is an experimental feature. This feature could ` +
       'change at any time';
  experimentalWarnings.add(feature);
  process.emitWarning(msg, 'ExperimentalWarning');
}

function filterDuplicateStrings(items, low) {
  const map = new SafeMap();
  for (let i = 0; i < items.length; i++) {
    const item = items[i];
    const key = StringPrototypeToLowerCase(item);
    if (low) {
      map.set(key, key);
    } else {
      map.set(key, item);
    }
  }
  return ArrayPrototypeSort(ArrayFrom(map.values()));
}

function cachedResult(fn) {
  let result;
  return () => {
    if (result === undefined)
      result = fn();
    return ArrayPrototypeSlice(result);
  };
}

// Useful for Wrapping an ES6 Class with a constructor Function that
// does not require the new keyword. For instance:
//   class A { constructor(x) {this.x = x;}}
//   const B = createClassWrapper(A);
//   B() instanceof A // true
//   B() instanceof B // true
function createClassWrapper(type) {
  function fn(...args) {
    return ReflectConstruct(type, args, new.target || type);
  }
  // Mask the wrapper function name and length values
  ObjectDefineProperties(fn, {
    name: { __proto__: null, value: type.name },
    length: { __proto__: null, value: type.length },
  });
  ObjectSetPrototypeOf(fn, type);
  fn.prototype = type.prototype;
  return fn;
}

let signalsToNamesMapping;
function getSignalsToNamesMapping() {
  if (signalsToNamesMapping !== undefined)
    return signalsToNamesMapping;

  signalsToNamesMapping = ObjectCreate(null);
  for (const key in signals) {
    signalsToNamesMapping[signals[key]] = key;
  }

  return signalsToNamesMapping;
}

function convertToValidSignal(signal) {
  if (typeof signal === 'number' && getSignalsToNamesMapping()[signal])
    return signal;

  if (typeof signal === 'string') {
    const signalName = signals[StringPrototypeToUpperCase(signal)];
    if (signalName) return signalName;
  }

  throw new ERR_UNKNOWN_SIGNAL(signal);
}

function getConstructorOf(obj) {
  while (obj) {
    const descriptor = ObjectGetOwnPropertyDescriptor(obj, 'constructor');
    if (descriptor !== undefined &&
        typeof descriptor.value === 'function' &&
        descriptor.value.name !== '') {
      return descriptor.value;
    }

    obj = ObjectGetPrototypeOf(obj);
  }

  return null;
}

function getSystemErrorName(err) {
  const entry = uvErrmapGet(err);
  return entry ? entry[0] : `Unknown system error ${err}`;
}

function getSystemErrorMap() {
  return lazyUv().getErrorMap();
}

const kCustomPromisifiedSymbol = SymbolFor('nodejs.util.promisify.custom');
const kCustomPromisifyArgsSymbol = Symbol('customPromisifyArgs');

let validateFunction;

function promisify(original) {
  // Lazy-load to avoid a circular dependency.
  if (validateFunction === undefined)
    ({ validateFunction } = require('internal/validators'));

  validateFunction(original, 'original');

  if (original[kCustomPromisifiedSymbol]) {
    const fn = original[kCustomPromisifiedSymbol];

    validateFunction(fn, 'util.promisify.custom');

    return ObjectDefineProperty(fn, kCustomPromisifiedSymbol, {
      __proto__: null,
      value: fn, enumerable: false, writable: false, configurable: true
    });
  }

  // Names to create an object from in case the callback receives multiple
  // arguments, e.g. ['bytesRead', 'buffer'] for fs.read.
  const argumentNames = original[kCustomPromisifyArgsSymbol];

  function fn(...args) {
    return new Promise((resolve, reject) => {
      ArrayPrototypePush(args, (err, ...values) => {
        if (err) {
          return reject(err);
        }
        if (argumentNames !== undefined && values.length > 1) {
          const obj = {};
          for (let i = 0; i < argumentNames.length; i++)
            obj[argumentNames[i]] = values[i];
          resolve(obj);
        } else {
          resolve(values[0]);
        }
      });
      ReflectApply(original, this, args);
    });
  }

  ObjectSetPrototypeOf(fn, ObjectGetPrototypeOf(original));

  ObjectDefineProperty(fn, kCustomPromisifiedSymbol, {
    __proto__: null,
    value: fn, enumerable: false, writable: false, configurable: true
  });
  return ObjectDefineProperties(
    fn,
    ObjectGetOwnPropertyDescriptors(original)
  );
}

promisify.custom = kCustomPromisifiedSymbol;

// The built-in Array#join is slower in v8 6.0
function join(output, separator) {
  let str = '';
  if (output.length !== 0) {
    const lastIndex = output.length - 1;
    for (let i = 0; i < lastIndex; i++) {
      // It is faster not to use a template string here
      str += output[i];
      str += separator;
    }
    str += output[lastIndex];
  }
  return str;
}

// As of V8 6.6, depending on the size of the array, this is anywhere
// between 1.5-10x faster than the two-arg version of Array#splice()
function spliceOne(list, index) {
  for (; index + 1 < list.length; index++)
    list[index] = list[index + 1];
  list.pop();
}

const kNodeModulesRE = /^(.*)[\\/]node_modules[\\/]/;

let getStructuredStack;

function isInsideNodeModules() {
  if (getStructuredStack === undefined) {
    // Lazy-load to avoid a circular dependency.
    const { runInNewContext } = require('vm');
    // Use `runInNewContext()` to get something tamper-proof and
    // side-effect-free. Since this is currently only used for a deprecated API,
    // the perf implications should be okay.
    getStructuredStack = runInNewContext(`(function() {
      try { Error.stackTraceLimit = Infinity; } catch {}
      return function structuredStack() {
        const e = new Error();
        overrideStackTrace.set(e, (err, trace) => trace);
        return e.stack;
      };
    })()`, { overrideStackTrace }, { filename: 'structured-stack' });
  }

  const stack = getStructuredStack();

  // Iterate over all stack frames and look for the first one not coming
  // from inside Node.js itself:
  if (ArrayIsArray(stack)) {
    for (const frame of stack) {
      const filename = frame.getFileName();
      // If a filename does not start with / or contain \,
      // it's likely from Node.js core.
      if (!RegExpPrototypeTest(/^\/|\\/, filename))
        continue;
      return RegExpPrototypeTest(kNodeModulesRE, filename);
    }
  }
  return false;
}

function once(callback) {
  let called = false;
  return function(...args) {
    if (called) return;
    called = true;
    ReflectApply(callback, this, args);
  };
}

let validateUint32;

function sleep(msec) {
  // Lazy-load to avoid a circular dependency.
  if (validateUint32 === undefined)
    ({ validateUint32 } = require('internal/validators'));

  validateUint32(msec, 'msec');
  _sleep(msec);
}

function createDeferredPromise() {
  let resolve;
  let reject;
  const promise = new Promise((res, rej) => {
    resolve = res;
    reject = rej;
  });

  return { promise, resolve, reject };
}

// https://heycam.github.io/webidl/#define-the-operations
function defineOperation(target, name, method) {
  ObjectDefineProperty(target, name, {
    __proto__: null,
    writable: true,
    enumerable: true,
    configurable: true,
    value: method
  });
}

// https://heycam.github.io/webidl/#es-interfaces
function exposeInterface(target, name, interfaceObject) {
  ObjectDefineProperty(target, name, {
    __proto__: null,
    writable: true,
    enumerable: false,
    configurable: true,
    value: interfaceObject
  });
}

let _DOMException;
const lazyDOMExceptionClass = () => {
  _DOMException ??= internalBinding('messaging').DOMException;
  return _DOMException;
};

const lazyDOMException = hideStackFrames((message, name) => {
  _DOMException ??= internalBinding('messaging').DOMException;
  return new _DOMException(message, name);
});

const kEnumerableProperty = ObjectCreate(null);
kEnumerableProperty.enumerable = true;
ObjectFreeze(kEnumerableProperty);

const kEmptyObject = ObjectFreeze(ObjectCreate(null));

module.exports = {
  assertCrypto,
  cachedResult,
  convertToValidSignal,
  createClassWrapper,
  createDeferredPromise,
  decorateErrorStack,
  defineOperation,
  deprecate,
  emitExperimentalWarning,
  exposeInterface,
  filterDuplicateStrings,
  getConstructorOf,
  getSystemErrorMap,
  getSystemErrorName,
  isError,
  isInsideNodeModules,
  join,
  lazyDOMException,
  lazyDOMExceptionClass,
  normalizeEncoding,
  once,
  promisify,
  sleep,
  spliceOne,
  toUSVString,
  removeColors,

  // Symbol used to customize promisify conversion
  customPromisifyArgs: kCustomPromisifyArgsSymbol,

  // Symbol used to provide a custom inspect function for an object as an
  // alternative to using 'inspect'
  customInspectSymbol: SymbolFor('nodejs.util.inspect.custom'),

  // Used by the buffer module to capture an internal reference to the
  // default isEncoding implementation, just in case userland overrides it.
  kIsEncodingSymbol: Symbol('kIsEncodingSymbol'),
  kVmBreakFirstLineSymbol: Symbol('kVmBreakFirstLineSymbol'),

  kEmptyObject,
  kEnumerableProperty,
};
