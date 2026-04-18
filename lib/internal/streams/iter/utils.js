'use strict';

const {
  Array,
  ArrayBufferPrototypeGetByteLength,
  ArrayPrototypeSlice,
  MathMax,
  MathMin,
  NumberMAX_SAFE_INTEGER,
  PromiseResolve,
  String,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  Uint8Array,
} = primordials;

const { TextEncoder } = require('internal/encoding');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_OPERATION_FAILED,
  },
} = require('internal/errors');
const { isError } = require('internal/util');

const { Buffer } = require('buffer');

const { isSharedArrayBuffer, isUint8Array } = require('internal/util/types');

const { validateOneOf } = require('internal/validators');

// Cached resolved promise to avoid allocating a new one on every sync fast-path.
const kResolvedPromise = PromiseResolve();

// Shared TextEncoder instance for string conversion.
const encoder = new TextEncoder();

// Default high water marks for push and multi-consumer streams. These values
// are somewhat arbitrary but have been tested across various workloads and
// appear to yield the best overall throughput/latency balance.

/** Default high water mark for push streams (single-consumer). */
const kPushDefaultHWM = 4;

/** Default high water mark for broadcast and share streams (multi-consumer). */
const kMultiConsumerDefaultHWM = 16;

/**
 * Clamp a high water mark to [1, MAX_SAFE_INTEGER].
 * @param {number} value
 * @returns {number}
 */
function clampHWM(value) {
  return MathMax(1, MathMin(NumberMAX_SAFE_INTEGER, value));
}

/**
 * Register a handler for an AbortSignal, handling the already-aborted case.
 * If the signal is already aborted, calls handler immediately.
 * Otherwise, adds a one-time 'abort' listener.
 * @param {AbortSignal} signal
 * @param {Function} handler
 */
function onSignalAbort(signal, handler) {
  if (signal.aborted) {
    handler();
  } else {
    signal.addEventListener('abort', handler, { __proto__: null, once: true });
  }
}

/**
 * Compute the minimum cursor across a set of consumers.
 * Returns fallback if the set is empty.
 * @param {Set} consumers - Set of objects with a `cursor` property
 * @param {number} fallback - Value to return when set is empty
 * @returns {number}
 */
function getMinCursor(consumers, fallback) {
  let min = Infinity;
  for (const consumer of consumers) {
    if (consumer.cursor < min) {
      min = consumer.cursor;
    }
  }
  return min === Infinity ? fallback : min;
}

/**
 * Convert a chunk (string or Uint8Array) to Uint8Array.
 * Strings are UTF-8 encoded.
 * @param {Uint8Array|string} chunk
 * @returns {Uint8Array}
 */
function toUint8Array(chunk) {
  if (typeof chunk === 'string') {
    return encoder.encode(chunk);
  }
  if (!isUint8Array(chunk)) {
    throw new ERR_INVALID_ARG_TYPE('chunk', ['string', 'Uint8Array'], chunk);
  }
  return chunk;
}

/**
 * Check if all chunks in an array are already Uint8Array (no strings).
 * Short-circuits on the first string found.
 * @param {Array<Uint8Array|string>} chunks
 * @returns {boolean}
 */
function allUint8Array(chunks) {
  // Ok, well, kind of. This is more a check for "no strings"...
  for (let i = 0; i < chunks.length; i++) {
    if (typeof chunks[i] === 'string') return false;
  }
  return true;
}

/**
 * Concatenate multiple Uint8Arrays into a single Uint8Array.
 * @param {Uint8Array[]} chunks
 * @returns {Uint8Array}
 */
function concatBytes(chunks) {
  // Empty stream: return zero-length Uint8Array
  if (chunks.length === 0) {
    return new Uint8Array(0);
  }
  // Single chunk: return directly if it covers the entire backing buffer
  if (chunks.length === 1) {
    const chunk = chunks[0];
    const buf = TypedArrayPrototypeGetBuffer(chunk);
    // SharedArrayBuffer is not available in primordials, so use
    // direct property access for its byteLength.
    const bufByteLength = isSharedArrayBuffer(buf) ?
      buf.byteLength :
      ArrayBufferPrototypeGetByteLength(buf);
    if (TypedArrayPrototypeGetByteOffset(chunk) === 0 &&
        TypedArrayPrototypeGetByteLength(chunk) === bufByteLength) {
      return chunk;
    }
  }
  // Multiple chunks or shared buffer: concatenate
  const buf = Buffer.concat(chunks);
  return new Uint8Array(
    TypedArrayPrototypeGetBuffer(buf),
    TypedArrayPrototypeGetByteOffset(buf),
    TypedArrayPrototypeGetByteLength(buf));
}

/**
 * Convert an array of chunks (strings or Uint8Arrays) to a Uint8Array[].
 * Always returns a fresh copy of the array.
 * @param {Array<Uint8Array|string>} chunks
 * @returns {Uint8Array[]}
 */
function convertChunks(chunks) {
  if (allUint8Array(chunks)) {
    return ArrayPrototypeSlice(chunks);
  }
  const len = chunks.length;
  const result = new Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = toUint8Array(chunks[i]);
  }
  return result;
}

/**
 * Wrap a caught value as an Error, converting non-Error values.
 * @param {unknown} error
 * @returns {Error}
 */
function wrapError(error) {
  return isError(error) ? error : new ERR_OPERATION_FAILED(String(error));
}

/**
 * Check if a value implements a Symbol-keyed protocol (has a function
 * at the given symbol key).
 * @param {unknown} value
 * @param {symbol} symbol
 * @returns {boolean}
 */
function hasProtocol(value, symbol) {
  return (
    value !== null &&
    typeof value === 'object' &&
    symbol in value &&
    typeof value[symbol] === 'function'
  );
}

/**
 * Check if a value is PullOptions (object without transform or write property).
 * @param {unknown} value
 * @returns {boolean}
 */
function isPullOptions(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !('transform' in value) &&
    !('write' in value)
  );
}

/**
 * Check if a value is a stateful transform object (has a transform method).
 * @param {unknown} value
 * @returns {boolean}
 */
function isTransformObject(value) {
  return typeof value?.transform === 'function';
}

/**
 * Check if a value is a valid transform (function or transform object).
 * @param {unknown} value
 * @returns {boolean}
 */
function isTransform(value) {
  return typeof value === 'function' || isTransformObject(value);
}

/**
 * Parse variadic arguments for pull/pullSync.
 * Returns { transforms, options }
 * @param {Array} args
 * @returns {{ transforms: Array, options: object|undefined }}
 */
function parsePullArgs(args) {
  if (args.length === 0) {
    return { __proto__: null, transforms: [], options: undefined };
  }

  let transforms;
  let options;
  const last = args[args.length - 1];
  if (isPullOptions(last)) {
    transforms = ArrayPrototypeSlice(args, 0, -1);
    options = last;
  } else {
    transforms = args;
    options = undefined;
  }

  for (let i = 0; i < transforms.length; i++) {
    if (!isTransform(transforms[i])) {
      throw new ERR_INVALID_ARG_TYPE(
        `transforms[${i}]`, ['Function', 'Object with transform()'],
        transforms[i]);
    }
  }

  return { __proto__: null, transforms, options };
}

/**
 * Validate backpressure option value.
 * @param {string} value
 */
function validateBackpressure(value) {
  validateOneOf(value, 'options.backpressure', [
    'strict',
    'block',
    'drop-oldest',
    'drop-newest',
  ]);
}

module.exports = {
  kMultiConsumerDefaultHWM,
  kPushDefaultHWM,
  kResolvedPromise,
  allUint8Array,
  clampHWM,
  concatBytes,
  convertChunks,
  getMinCursor,
  hasProtocol,
  isPullOptions,
  isTransform,
  isTransformObject,
  onSignalAbort,
  parsePullArgs,
  toUint8Array,
  validateBackpressure,
  wrapError,
};
