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

const {
  ArrayIsArray,
  ArrayPrototypePop,
  ArrayPrototypePush,
  Error,
  ErrorCaptureStackTrace,
  FunctionPrototypeBind,
  NumberIsSafeInteger,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptors,
  ObjectKeys,
  ObjectSetPrototypeOf,
  ObjectValues,
  ReflectApply,
  StringPrototypeToWellFormed,
} = primordials;

const {
  ErrnoException,
  ExceptionWithHostPort,
  codes: {
    ERR_FALSY_VALUE_REJECTION,
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
  },
  isErrorStackTraceLimitWritable,
} = require('internal/errors');
const {
  format,
  formatWithOptions,
  inspect,
  stripVTControlCharacters,
} = require('internal/util/inspect');
const { debuglog } = require('internal/util/debuglog');
const {
  validateBoolean,
  validateFunction,
  validateNumber,
  validateString,
  validateOneOf,
  validateObject,
  validateInteger,
} = require('internal/validators');
const {
  isReadableStream,
  isWritableStream,
  isNodeStream,
} = require('internal/streams/utils');
const types = require('internal/util/types');

let utilColors;
function lazyUtilColors() {
  utilColors ??= require('internal/util/colors');
  return utilColors;
}
const { getOptionValue } = require('internal/options');

const binding = internalBinding('util');

const {
  convertProcessSignalToExitCode,
  deprecate: internalDeprecate,
  getLazy,
  getSystemErrorMap,
  getSystemErrorName: internalErrorName,
  getSystemErrorMessage: internalErrorMessage,
  promisify,
  defineLazyProperties,
} = require('internal/util');

let abortController;

function lazyAbortController() {
  abortController ??= require('internal/abort_controller');
  return abortController;
}

let internalDeepEqual;

// Pre-computed ANSI escape code constants
const kEscape = '\u001b[';
const kEscapeEnd = 'm';

// Codes for dim (2) and bold (1) - these share close code 22
const kDimCode = 2;
const kBoldCode = 1;

let styleCache;

function getStyleCache() {
  if (styleCache === undefined) {
    styleCache = { __proto__: null };
    const colors = inspect.colors;
    for (const key of ObjectKeys(colors)) {
      const codes = colors[key];
      if (codes) {
        const openNum = codes[0];
        const closeNum = codes[1];
        styleCache[key] = {
          __proto__: null,
          openSeq: kEscape + openNum + kEscapeEnd,
          closeSeq: kEscape + closeNum + kEscapeEnd,
          keepClose: openNum === kDimCode || openNum === kBoldCode,
        };
      }
    }
  }
  return styleCache;
}

function replaceCloseCode(str, closeSeq, openSeq, keepClose) {
  const closeLen = closeSeq.length;
  let index = str.indexOf(closeSeq);
  if (index === -1) return str;

  let result = '';
  let lastIndex = 0;
  const replacement = keepClose ? closeSeq + openSeq : openSeq;

  do {
    const afterClose = index + closeLen;
    if (afterClose < str.length) {
      result += str.slice(lastIndex, index) + replacement;
      lastIndex = afterClose;
    } else {
      break;
    }
    index = str.indexOf(closeSeq, lastIndex);
  } while (index !== -1);

  return result + str.slice(lastIndex);
}

/**
 * @param {string | string[]} format
 * @param {string} text
 * @param {object} [options]
 * @param {boolean} [options.validateStream] - Whether to validate the stream.
 * @param {Stream} [options.stream] - The stream used for validation.
 * @returns {string}
 */
function styleText(format, text, options) {
  const validateStream = options?.validateStream ?? true;
  const cache = getStyleCache();

  // Fast path: single format string with validateStream=false
  if (!validateStream && typeof format === 'string' && typeof text === 'string') {
    if (format === 'none') return text;
    const style = cache[format];
    if (style !== undefined) {
      const processed = replaceCloseCode(text, style.closeSeq, style.openSeq, style.keepClose);
      return style.openSeq + processed + style.closeSeq;
    }
  }

  validateString(text, 'text');
  if (options !== undefined) {
    validateObject(options, 'options');
  }
  validateBoolean(validateStream, 'options.validateStream');

  let skipColorize;
  if (validateStream) {
    const stream = options?.stream ?? process.stdout;
    if (
      !isReadableStream(stream) &&
      !isWritableStream(stream) &&
      !isNodeStream(stream)
    ) {
      throw new ERR_INVALID_ARG_TYPE('stream', ['ReadableStream', 'WritableStream', 'Stream'], stream);
    }
    skipColorize = !lazyUtilColors().shouldColorize(stream);
  }

  const formatArray = ArrayIsArray(format) ? format : [format];

  let openCodes = '';
  let closeCodes = '';
  let processedText = text;

  for (const key of formatArray) {
    if (key === 'none') continue;
    const style = cache[key];
    if (style === undefined) {
      validateOneOf(key, 'format', ObjectKeys(inspect.colors));
    }
    openCodes += style.openSeq;
    closeCodes = style.closeSeq + closeCodes;
    processedText = replaceCloseCode(processedText, style.closeSeq, style.openSeq, style.keepClose);
  }

  if (skipColorize) return text;

  return `${openCodes}${processedText}${closeCodes}`;
}

/**
 * Inherit the prototype methods from one constructor into another.
 *
 * The Function.prototype.inherits from lang.js rewritten as a standalone
 * function (not on Function.prototype). NOTE: If this file is to be loaded
 * during bootstrapping this function needs to be rewritten using some native
 * functions as prototype setup using normal JavaScript does not work as
 * expected during bootstrapping (see mirror.js in r114903).
 * @param {Function} ctor Constructor function which needs to inherit the
 *   prototype.
 * @param {Function} superCtor Constructor function to inherit prototype from.
 * @throws {TypeError} Will error if either constructor is null, or if
 * the super constructor lacks a prototype.
 */
function inherits(ctor, superCtor) {

  if (ctor === undefined || ctor === null)
    throw new ERR_INVALID_ARG_TYPE('ctor', 'Function', ctor);

  if (superCtor === undefined || superCtor === null)
    throw new ERR_INVALID_ARG_TYPE('superCtor', 'Function', superCtor);

  if (superCtor.prototype === undefined) {
    throw new ERR_INVALID_ARG_TYPE('superCtor.prototype',
                                   'Object', superCtor.prototype);
  }
  ObjectDefineProperty(ctor, 'super_', {
    __proto__: null,
    value: superCtor,
    writable: true,
    configurable: true,
  });
  ObjectSetPrototypeOf(ctor.prototype, superCtor.prototype);
}

/**
 * @deprecated since v6.0.0
 * @template T
 * @template S
 * @param {T} target
 * @param {S} source
 * @returns {(T & S) | null}
 */
function _extend(target, source) {
  // Don't do anything if source isn't an object
  if (source === null || typeof source !== 'object') return target;

  const keys = ObjectKeys(source);
  let i = keys.length;
  while (i--) {
    target[keys[i]] = source[keys[i]];
  }
  return target;
}

const callbackifyOnRejected = (reason, cb) => {
  // `!reason` guard inspired by bluebird (Ref: https://goo.gl/t5IS6M).
  // Because `null` is a special error value in callbacks which means "no error
  // occurred", we error-wrap so the callback consumer can distinguish between
  // "the promise rejected with null" or "the promise fulfilled with undefined".
  if (!reason) {
    reason = new ERR_FALSY_VALUE_REJECTION.HideStackFramesError(reason);
    ErrorCaptureStackTrace(reason, callbackifyOnRejected);
  }
  return cb(reason);
};

/**
 * Converts a Promise-returning function to callback style
 * @param {Function} original
 * @returns {Function}
 */
function callbackify(original) {
  validateFunction(original, 'original');

  // We DO NOT return the promise as it gives the user a false sense that
  // the promise is actually somehow related to the callback's execution
  // and that the callback throwing will reject the promise.
  function callbackified(...args) {
    const maybeCb = ArrayPrototypePop(args);
    validateFunction(maybeCb, 'last argument');
    const cb = FunctionPrototypeBind(maybeCb, this);
    // In true node style we process the callback on `nextTick` with all the
    // implications (stack, `uncaughtException`, `async_hooks`)
    ReflectApply(original, this, args)
      .then((ret) => process.nextTick(cb, null, ret),
            (rej) => process.nextTick(callbackifyOnRejected, rej, cb));
  }

  const descriptors = ObjectGetOwnPropertyDescriptors(original);
  // It is possible to manipulate a functions `length` or `name` property. This
  // guards against the manipulation.
  if (typeof descriptors.length.value === 'number') {
    descriptors.length.value++;
  }
  if (typeof descriptors.name.value === 'string') {
    descriptors.name.value += 'Callbackified';
  }
  const propertiesValues = ObjectValues(descriptors);
  for (let i = 0; i < propertiesValues.length; i++) {
  // We want to use null-prototype objects to not rely on globally mutable
  // %Object.prototype%.
    ObjectSetPrototypeOf(propertiesValues[i], null);
  }
  ObjectDefineProperties(callbackified, descriptors);
  return callbackified;
}

/**
 * @param {number} err
 * @returns {string}
 */
function getSystemErrorMessage(err) {
  validateNumber(err, 'err');
  if (err >= 0 || !NumberIsSafeInteger(err)) {
    throw new ERR_OUT_OF_RANGE('err', 'a negative integer', err);
  }
  return internalErrorMessage(err);
}

/**
 * @param {number} err
 * @returns {string}
 */
function getSystemErrorName(err) {
  validateNumber(err, 'err');
  if (err >= 0 || !NumberIsSafeInteger(err)) {
    throw new ERR_OUT_OF_RANGE('err', 'a negative integer', err);
  }
  return internalErrorName(err);
}

function _errnoException(...args) {
  if (isErrorStackTraceLimitWritable()) {
    const limit = Error.stackTraceLimit;
    Error.stackTraceLimit = 0;
    const e = new ErrnoException(...args);
    Error.stackTraceLimit = limit;
    ErrorCaptureStackTrace(e, _errnoException);
    return e;
  }
  return new ErrnoException(...args);
}

function _exceptionWithHostPort(...args) {
  if (isErrorStackTraceLimitWritable()) {
    const limit = Error.stackTraceLimit;
    Error.stackTraceLimit = 0;
    const e = new ExceptionWithHostPort(...args);
    Error.stackTraceLimit = limit;
    ErrorCaptureStackTrace(e, _exceptionWithHostPort);
    return e;
  }
  return new ExceptionWithHostPort(...args);
}

/**
 * Parses the content of a `.env` file.
 * @param {string} content
 * @returns {Record<string, string>}
 */
function parseEnv(content) {
  validateString(content, 'content');
  return binding.parseEnv(content);
}

const lazySourceMap = getLazy(() => require('internal/source_map/source_map_cache'));

/**
 * @typedef {object} CallSite // The call site
 * @property {string} scriptName // The name of the resource that contains the
 *   script for the function for this StackFrame
 * @property {string} functionName // The name of the function associated with this stack frame
 * @property {number} lineNumber // The number, 1-based, of the line for the associate function call
 * @property {number} columnNumber // The 1-based column offset on the line for the associated function call
 */

/**
 * @param {CallSite} callSite // The call site object to reconstruct from source map
 * @returns {CallSite | undefined} // The reconstructed call site object
 */
function reconstructCallSite(callSite) {
  const { scriptName, lineNumber, columnNumber } = callSite;
  const sourceMap = lazySourceMap().findSourceMap(scriptName);
  if (!sourceMap) return;
  const entry = sourceMap.findEntry(lineNumber - 1, columnNumber - 1);
  if (!entry?.originalSource) return;
  return {
    __proto__: null,
    // If the name is not found, it is an empty string to match the behavior of `util.getCallSite()`
    functionName: entry.name ?? '',
    scriptName: entry.originalSource,
    lineNumber: entry.originalLine + 1,
    column: entry.originalColumn + 1,
    columnNumber: entry.originalColumn + 1,
  };
}

/**
 *
 * The call site array to map
 * @param {CallSite[]} callSites
 *   Array of objects with the reconstructed call site
 * @returns {CallSite[]}
 */
function mapCallSite(callSites) {
  const result = [];
  for (let i = 0; i < callSites.length; ++i) {
    const callSite = callSites[i];
    const found = reconstructCallSite(callSite);
    ArrayPrototypePush(result, found ?? callSite);
  }
  return result;
}

/**
 * @typedef {object} CallSiteOptions // The call site options
 * @property {boolean} sourceMap // Enable source map support
 */

/**
 * Returns the callSite
 * @param {number} frameCount
 * @param {CallSiteOptions} options
 * @returns {CallSite[]}
 */
function getCallSites(frameCount = 10, options) {
  // If options is not provided check if frameCount is an object
  if (options === undefined) {
    if (typeof frameCount === 'object') {
      // If frameCount is an object, it is the options object
      options = frameCount;
      validateObject(options, 'options');
      if (options.sourceMap !== undefined) {
        validateBoolean(options.sourceMap, 'options.sourceMap');
      }
      frameCount = 10;
    } else {
      // If options is not provided, set it to an empty object
      options = {};
    };
  } else {
    // If options is provided, validate it
    validateObject(options, 'options');
    if (options.sourceMap !== undefined) {
      validateBoolean(options.sourceMap, 'options.sourceMap');
    }
  }

  // Using kDefaultMaxCallStackSizeToCapture as reference
  validateInteger(frameCount, 'frameCount', 1, 200);
  // If options.sourceMaps is true or if sourceMaps are enabled but the option.sourceMaps is not set explicitly to false
  if (options.sourceMap === true || (getOptionValue('--enable-source-maps') && options.sourceMap !== false)) {
    return mapCallSite(binding.getCallSites(frameCount));
  }
  return binding.getCallSites(frameCount);
};

// Public util.deprecate API
function deprecate(fn, msg, code, { modifyPrototype } = {}) {
  return internalDeprecate(fn, msg, code, undefined, modifyPrototype);
}

// Keep the `exports =` so that various functions can still be monkeypatched
module.exports = {
  _errnoException,
  _exceptionWithHostPort,
  _extend: internalDeprecate(_extend,
                             'The `util._extend` API is deprecated. Please use Object.assign() instead.',
                             'DEP0060'),
  callbackify,
  convertProcessSignalToExitCode,
  debug: debuglog,
  debuglog,
  deprecate,
  format,
  styleText,
  formatWithOptions,
  getCallSites,
  getSystemErrorMap,
  getSystemErrorName,
  getSystemErrorMessage,
  inherits,
  inspect,
  isArray: internalDeprecate(ArrayIsArray,
                             'The `util.isArray` API is deprecated. Please use `Array.isArray()` instead.',
                             'DEP0044'),
  isDeepStrictEqual(a, b, skipPrototype) {
    if (internalDeepEqual === undefined) {
      internalDeepEqual = require('internal/util/comparisons').isDeepStrictEqual;
    }
    return internalDeepEqual(a, b, skipPrototype);
  },
  promisify,
  stripVTControlCharacters,
  toUSVString(input) {
    return StringPrototypeToWellFormed(`${input}`);
  },
  get transferableAbortSignal() {
    return lazyAbortController().transferableAbortSignal;
  },
  get transferableAbortController() {
    return lazyAbortController().transferableAbortController;
  },
  get aborted() {
    return lazyAbortController().aborted;
  },
  types,
  parseEnv,
};

defineLazyProperties(
  module.exports,
  'internal/util/parse_args/parse_args',
  ['parseArgs'],
);

defineLazyProperties(
  module.exports,
  'internal/encoding',
  ['TextDecoder', 'TextEncoder'],
);

defineLazyProperties(
  module.exports,
  'internal/mime',
  ['MIMEType', 'MIMEParams'],
);

defineLazyProperties(
  module.exports,
  'internal/util/diff',
  ['diff'],
);

defineLazyProperties(
  module.exports,
  'internal/util/trace_sigint',
  ['setTraceSigInt'],
);
