'use strict';

// New Streams API - from() and fromSync()
//
// Creates normalized byte stream iterables from various input types.
// Handles recursive flattening of nested iterables and protocol conversions.

const {
  ArrayBuffer,
  ArrayBufferIsView,
  ArrayIsArray,
  ArrayPrototypeEvery,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ObjectPrototypeToString,
  Promise,
  SymbolAsyncIterator,
  SymbolIterator,
  SymbolToPrimitive,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const { TextEncoder } = require('internal/encoding');

const {
  toStreamable,
  toAsyncStreamable,
} = require('internal/streams/new/types');

// Shared TextEncoder instance for string conversion.
const encoder = new TextEncoder();

// Maximum number of chunks to yield per batch from from(Uint8Array[]).
// Bounds peak memory when arrays flow through transforms, which must
// allocate output for the entire batch at once.
const FROM_BATCH_SIZE = 128;

// =============================================================================
// Type Guards and Detection
// =============================================================================

/**
 * Check if value is a primitive chunk (string, ArrayBuffer, or ArrayBufferView).
 * @returns {boolean}
 */
function isPrimitiveChunk(value) {
  if (typeof value === 'string') return true;
  if (value instanceof ArrayBuffer) return true;
  if (ArrayBufferIsView(value)) return true;
  return false;
}

/**
 * Check if value implements ToStreamable protocol.
 * @returns {boolean}
 */
function isToStreamable(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    toStreamable in value &&
    typeof value[toStreamable] === 'function'
  );
}

/**
 * Check if value implements ToAsyncStreamable protocol.
 * @returns {boolean}
 */
function isToAsyncStreamable(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    toAsyncStreamable in value &&
    typeof value[toAsyncStreamable] === 'function'
  );
}

/**
 * Check if value is a sync iterable (has Symbol.iterator).
 * @returns {boolean}
 */
function isSyncIterable(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    SymbolIterator in value &&
    typeof value[SymbolIterator] === 'function'
  );
}

/**
 * Check if value is an async iterable (has Symbol.asyncIterator).
 * @returns {boolean}
 */
function isAsyncIterable(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    SymbolAsyncIterator in value &&
    typeof value[SymbolAsyncIterator] === 'function'
  );
}

/**
 * Check if object has a custom toString() (not Object.prototype.toString).
 * @returns {boolean}
 */
function hasCustomToString(obj) {
  const toString = obj.toString;
  return typeof toString === 'function' &&
         toString !== ObjectPrototypeToString;
}

/**
 * Check if object has Symbol.toPrimitive.
 * @returns {boolean}
 */
function hasToPrimitive(obj) {
  return (
    SymbolToPrimitive in obj &&
    typeof obj[SymbolToPrimitive] === 'function'
  );
}

// =============================================================================
// Primitive Conversion
// =============================================================================

/**
 * Convert a primitive chunk to Uint8Array.
 * - string: UTF-8 encoded
 * - ArrayBuffer: wrapped as Uint8Array view (no copy)
 * - ArrayBufferView: converted to Uint8Array view of same memory
 * @param {string|ArrayBuffer|ArrayBufferView} chunk
 * @returns {Uint8Array}
 */
function primitiveToUint8Array(chunk) {
  if (typeof chunk === 'string') {
    return encoder.encode(chunk);
  }
  if (chunk instanceof ArrayBuffer) {
    return new Uint8Array(chunk);
  }
  if (chunk instanceof Uint8Array) {
    return chunk;
  }
  // Other ArrayBufferView types (Int8Array, DataView, etc.)
  return new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength);
}

/**
 * Try to coerce an object to string using custom methods.
 * Returns null if object has no custom string coercion.
 * @returns {string|null}
 */
function tryStringCoercion(obj) {
  // Check for Symbol.toPrimitive first
  if (hasToPrimitive(obj)) {
    const toPrimitive = obj[SymbolToPrimitive];
    const result = toPrimitive.call(obj, 'string');
    if (typeof result === 'string') {
      return result;
    }
    // toPrimitive returned non-string, fall through to toString
  }

  // Check for custom toString
  if (hasCustomToString(obj)) {
    const result = obj.toString();
    return result;
  }

  return null;
}

// =============================================================================
// Sync Normalization (for fromSync and sync contexts)
// =============================================================================

/**
 * Normalize a sync streamable yield value to Uint8Array chunks.
 * Recursively flattens arrays, iterables, and protocol conversions.
 * @yields {Uint8Array}
 */
function* normalizeSyncValue(value) {
  // Handle primitives
  if (isPrimitiveChunk(value)) {
    yield primitiveToUint8Array(value);
    return;
  }

  // Handle ToStreamable protocol
  if (isToStreamable(value)) {
    const result = value[toStreamable]();
    yield* normalizeSyncValue(result);
    return;
  }

  // Handle arrays (which are also iterable, but check first for efficiency)
  if (ArrayIsArray(value)) {
    for (let i = 0; i < value.length; i++) {
      yield* normalizeSyncValue(value[i]);
    }
    return;
  }

  // Handle other sync iterables
  if (isSyncIterable(value)) {
    for (const item of value) {
      yield* normalizeSyncValue(item);
    }
    return;
  }

  // Try string coercion for objects with custom toString/toPrimitive
  if (typeof value === 'object' && value !== null) {
    const str = tryStringCoercion(value);
    if (str !== null) {
      yield encoder.encode(str);
      return;
    }
  }

  // Reject: no valid conversion
  throw new ERR_INVALID_ARG_TYPE(
    'value',
    ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable'],
    value,
  );
}

/**
 * Check if value is already a Uint8Array[] batch (fast path).
 * @returns {boolean}
 */
function isUint8ArrayBatch(value) {
  if (!ArrayIsArray(value)) return false;
  if (value.length === 0) return true;
  // Check first element - if it's a Uint8Array, assume the rest are too
  return value[0] instanceof Uint8Array;
}

/**
 * Normalize a sync streamable source, yielding batches of Uint8Array.
 * @param {Iterable} source
 * @yields {Uint8Array[]}
 */
function* normalizeSyncSource(source) {
  for (const value of source) {
    // Fast path 1: value is already a Uint8Array[] batch
    if (isUint8ArrayBatch(value)) {
      if (value.length > 0) {
        yield value;
      }
      continue;
    }
    // Fast path 2: value is a single Uint8Array (very common)
    if (value instanceof Uint8Array) {
      yield [value];
      continue;
    }
    // Slow path: normalize the value
    const batch = [];
    for (const chunk of normalizeSyncValue(value)) {
      ArrayPrototypePush(batch, chunk);
    }
    if (batch.length > 0) {
      yield batch;
    }
  }
}

// =============================================================================
// Async Normalization (for from and async contexts)
// =============================================================================

/**
 * Normalize an async streamable yield value to Uint8Array chunks.
 * Recursively flattens arrays, iterables, async iterables, promises,
 * and protocol conversions.
 * @yields {Uint8Array}
 */
async function* normalizeAsyncValue(value) {
  // Handle promises first
  if (value instanceof Promise) {
    const resolved = await value;
    yield* normalizeAsyncValue(resolved);
    return;
  }

  // Handle primitives
  if (isPrimitiveChunk(value)) {
    yield primitiveToUint8Array(value);
    return;
  }

  // Handle ToAsyncStreamable protocol (check before ToStreamable)
  if (isToAsyncStreamable(value)) {
    const result = value[toAsyncStreamable]();
    if (result instanceof Promise) {
      yield* normalizeAsyncValue(await result);
    } else {
      yield* normalizeAsyncValue(result);
    }
    return;
  }

  // Handle ToStreamable protocol
  if (isToStreamable(value)) {
    const result = value[toStreamable]();
    yield* normalizeAsyncValue(result);
    return;
  }

  // Handle arrays (which are also iterable, but check first for efficiency)
  if (ArrayIsArray(value)) {
    for (let i = 0; i < value.length; i++) {
      yield* normalizeAsyncValue(value[i]);
    }
    return;
  }

  // Handle async iterables (check before sync iterables since some objects
  // have both)
  if (isAsyncIterable(value)) {
    for await (const item of value) {
      yield* normalizeAsyncValue(item);
    }
    return;
  }

  // Handle sync iterables
  if (isSyncIterable(value)) {
    for (const item of value) {
      yield* normalizeAsyncValue(item);
    }
    return;
  }

  // Try string coercion for objects with custom toString/toPrimitive
  if (typeof value === 'object' && value !== null) {
    const str = tryStringCoercion(value);
    if (str !== null) {
      yield encoder.encode(str);
      return;
    }
  }

  // Reject: no valid conversion
  throw new ERR_INVALID_ARG_TYPE(
    'value',
    ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable', 'AsyncIterable'],
    value,
  );
}

/**
 * Normalize an async streamable source, yielding batches of Uint8Array.
 * @param {AsyncIterable|Iterable} source
 * @yields {Uint8Array[]}
 */
async function* normalizeAsyncSource(source) {
  // Prefer async iteration if available
  if (isAsyncIterable(source)) {
    for await (const value of source) {
      // Fast path 1: value is already a Uint8Array[] batch
      if (isUint8ArrayBatch(value)) {
        if (value.length > 0) {
          yield value;
        }
        continue;
      }
      // Fast path 2: value is a single Uint8Array (very common)
      if (value instanceof Uint8Array) {
        yield [value];
        continue;
      }
      // Slow path: normalize the value
      const batch = [];
      for await (const chunk of normalizeAsyncValue(value)) {
        ArrayPrototypePush(batch, chunk);
      }
      if (batch.length > 0) {
        yield batch;
      }
    }
    return;
  }

  // Fall back to sync iteration - batch all sync values together
  if (isSyncIterable(source)) {
    const batch = [];

    for (const value of source) {
      // Fast path 1: value is already a Uint8Array[] batch
      if (isUint8ArrayBatch(value)) {
        // Flush any accumulated batch first
        if (batch.length > 0) {
          yield ArrayPrototypeSlice(batch);
          batch.length = 0;
        }
        if (value.length > 0) {
          yield value;
        }
        continue;
      }
      // Fast path 2: value is a single Uint8Array (very common)
      if (value instanceof Uint8Array) {
        ArrayPrototypePush(batch, value);
        continue;
      }
      // Slow path: normalize the value - must flush and yield individually
      if (batch.length > 0) {
        yield ArrayPrototypeSlice(batch);
        batch.length = 0;
      }
      const asyncBatch = [];
      for await (const chunk of normalizeAsyncValue(value)) {
        ArrayPrototypePush(asyncBatch, chunk);
      }
      if (asyncBatch.length > 0) {
        yield asyncBatch;
      }
    }

    // Yield any remaining batched values
    if (batch.length > 0) {
      yield batch;
    }
    return;
  }

  throw new ERR_INVALID_ARG_TYPE(
    'source',
    ['Iterable', 'AsyncIterable'],
    source,
  );
}

// =============================================================================
// Public API: from() and fromSync()
// =============================================================================

/**
 * Create a SyncByteStreamReadable from a ByteInput or SyncStreamable.
 * @param {string|ArrayBuffer|ArrayBufferView|Iterable} input
 * @returns {Iterable<Uint8Array[]>}
 */
function fromSync(input) {
  // Check for primitives first (ByteInput)
  if (isPrimitiveChunk(input)) {
    const chunk = primitiveToUint8Array(input);
    return {
      *[SymbolIterator]() {
        yield [chunk];
      },
    };
  }

  // Fast path: Uint8Array[] - yield in bounded sub-batches.
  // Yielding the entire array as one batch forces downstream transforms
  // to process all data at once, causing peak memory proportional to total
  // data volume. Sub-batching keeps peak memory bounded while preserving
  // the throughput benefit of batched processing.
  if (ArrayIsArray(input)) {
    if (input.length === 0) {
      return {
        *[SymbolIterator]() {
          // Empty - yield nothing
        },
      };
    }
    // Check if it's an array of Uint8Array (common case)
    if (input[0] instanceof Uint8Array) {
      const allUint8 = ArrayPrototypeEvery(input,
                                           (item) => item instanceof Uint8Array);
      if (allUint8) {
        const batch = input;
        return {
          *[SymbolIterator]() {
            if (batch.length <= FROM_BATCH_SIZE) {
              yield batch;
            } else {
              for (let i = 0; i < batch.length; i += FROM_BATCH_SIZE) {
                yield ArrayPrototypeSlice(batch, i, i + FROM_BATCH_SIZE);
              }
            }
          },
        };
      }
    }
  }

  // Must be a SyncStreamable
  if (!isSyncIterable(input)) {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable'],
      input,
    );
  }

  return {
    *[SymbolIterator]() {
      yield* normalizeSyncSource(input);
    },
  };
}

/**
 * Create a ByteStreamReadable from a ByteInput or Streamable.
 * @param {string|ArrayBuffer|ArrayBufferView|Iterable|AsyncIterable} input
 * @returns {AsyncIterable<Uint8Array[]>}
 */
function from(input) {
  // Check for primitives first (ByteInput)
  if (isPrimitiveChunk(input)) {
    const chunk = primitiveToUint8Array(input);
    return {
      async *[SymbolAsyncIterator]() {
        yield [chunk];
      },
    };
  }

  // Fast path: Uint8Array[] - yield in bounded sub-batches.
  // Yielding the entire array as one batch forces downstream transforms
  // to process all data at once, causing peak memory proportional to total
  // data volume. Sub-batching keeps peak memory bounded while preserving
  // the throughput benefit of batched processing.
  if (ArrayIsArray(input)) {
    if (input.length === 0) {
      return {
        async *[SymbolAsyncIterator]() {
          // Empty - yield nothing
        },
      };
    }
    if (input[0] instanceof Uint8Array) {
      const allUint8 = ArrayPrototypeEvery(input,
                                           (item) => item instanceof Uint8Array);
      if (allUint8) {
        const batch = input;
        return {
          async *[SymbolAsyncIterator]() {
            if (batch.length <= FROM_BATCH_SIZE) {
              yield batch;
            } else {
              for (let i = 0; i < batch.length; i += FROM_BATCH_SIZE) {
                yield ArrayPrototypeSlice(batch, i, i + FROM_BATCH_SIZE);
              }
            }
          },
        };
      }
    }
  }

  // Must be a Streamable (sync or async iterable)
  if (!isSyncIterable(input) && !isAsyncIterable(input)) {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable', 'AsyncIterable'],
      input,
    );
  }

  return {
    async *[SymbolAsyncIterator]() {
      yield* normalizeAsyncSource(input);
    },
  };
}

// =============================================================================
// Exports
// =============================================================================

module.exports = {
  from,
  fromSync,
  // Internal helpers used by pull, pipeTo, etc.
  normalizeSyncValue,
  normalizeSyncSource,
  normalizeAsyncValue,
  normalizeAsyncSource,
  isPrimitiveChunk,
  isToStreamable,
  isToAsyncStreamable,
  isSyncIterable,
  isAsyncIterable,
  isUint8ArrayBatch,
  primitiveToUint8Array,
};
