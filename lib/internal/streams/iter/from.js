'use strict';

// New Streams API - from() and fromSync()
//
// Creates normalized byte stream iterables from various input types.
// Handles recursive flattening of nested iterables and protocol conversions.

const {
  ArrayBufferIsView,
  ArrayIsArray,
  ArrayPrototypeEvery,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  DataViewPrototypeGetBuffer,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetByteOffset,
  FunctionPrototypeCall,
  SymbolAsyncIterator,
  SymbolIterator,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const {
  isAnyArrayBuffer,
  isDataView,
  isPromise,
  isUint8Array,
} = require('internal/util/types');

const {
  kValidatedSource,
  toStreamable,
  toAsyncStreamable,
} = require('internal/streams/iter/types');

const {
  hasProtocol,
  toUint8Array,
} = require('internal/streams/iter/utils');

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
  return typeof value === 'string' || isAnyArrayBuffer(value) || ArrayBufferIsView(value);
}

/**
 * Check if value is a sync iterable (has Symbol.iterator).
 * @returns {boolean}
 */
function isSyncIterable(value) {
  // We do not consider regular strings to be sync iterables in this context.
  // We don't care about boxed strings (String objects) since they are uncommon.
  return typeof value !== 'string' &&
      typeof value?.[SymbolIterator] === 'function';
}

/**
 * Check if value is an async iterable (has Symbol.asyncIterator).
 * @returns {boolean}
 */
function isAsyncIterable(value) {
  return typeof value?.[SymbolAsyncIterator] === 'function';
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
    return toUint8Array(chunk);
  }
  if (isAnyArrayBuffer(chunk)) {
    return new Uint8Array(chunk);
  }
  if (isUint8Array(chunk)) {
    return chunk;
  }
  // Other ArrayBufferView types (Int8Array, DataView, etc.)
  if (isDataView(chunk)) {
    return new Uint8Array(
      DataViewPrototypeGetBuffer(chunk),
      DataViewPrototypeGetByteOffset(chunk),
      DataViewPrototypeGetByteLength(chunk),
    );
  }
  return new Uint8Array(
    TypedArrayPrototypeGetBuffer(chunk),
    TypedArrayPrototypeGetByteOffset(chunk),
    TypedArrayPrototypeGetByteLength(chunk),
  );
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
  if (hasProtocol(value, toStreamable)) {
    const result = FunctionPrototypeCall(value[toStreamable], value);
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

  // Reject: no valid conversion
  throw new ERR_INVALID_ARG_TYPE(
    'value',
    ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable', 'toStreamable'],
    value,
  );
}

/**
 * Check if value is already a Uint8Array[] batch (fast path).
 * @returns {boolean}
 */
function isUint8ArrayBatch(value) {
  if (!ArrayIsArray(value)) return false;
  const len = value.length;
  if (len === 0) return true;
  // Fast path: single-element batch (most common from transforms)
  if (len === 1) return isUint8Array(value[0]);
  // Check first and last before iterating all elements
  if (!isUint8Array(value[0]) || !isUint8Array(value[len - 1])) return false;
  if (len === 2) return true;
  for (let i = 1; i < len - 1; i++) {
    if (!isUint8Array(value[i])) return false;
  }
  return true;
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
    if (isUint8Array(value)) {
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
  if (isPromise(value)) {
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
  if (hasProtocol(value, toAsyncStreamable)) {
    const result = FunctionPrototypeCall(value[toAsyncStreamable], value);
    if (isPromise(result)) {
      yield* normalizeAsyncValue(await result);
    } else {
      yield* normalizeAsyncValue(result);
    }
    return;
  }

  // Handle ToStreamable protocol
  if (hasProtocol(value, toStreamable)) {
    const result = FunctionPrototypeCall(value[toStreamable], value);
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

  // Reject: no valid conversion
  throw new ERR_INVALID_ARG_TYPE(
    'value',
    ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable', 'AsyncIterable',
     'toStreamable', 'toAsyncStreamable'],
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
      if (isUint8Array(value)) {
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
      if (isUint8Array(value)) {
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
  if (input == null) {
    throw new ERR_INVALID_ARG_TYPE('input', 'a non-null value', input);
  }

  // Check for primitives first (ByteInput)
  if (isPrimitiveChunk(input)) {
    const chunk = primitiveToUint8Array(input);
    return {
      __proto__: null,
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
        __proto__: null,
        *[SymbolIterator]() {
          // Empty - yield nothing
        },
      };
    }
    // Check if it's an array of Uint8Array (common case)
    if (isUint8Array(input[0])) {
      const allUint8 = ArrayPrototypeEvery(input, isUint8Array);
      if (allUint8) {
        const batch = input;
        return {
          __proto__: null,
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

  // Check toStreamable protocol (takes precedence over iteration protocols).
  // toAsyncStreamable is ignored entirely in fromSync.
  if (typeof input[toStreamable] === 'function') {
    return fromSync(input[toStreamable]());
  }

  // Reject explicit async inputs
  if (isAsyncIterable(input)) {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      'a synchronous input (not AsyncIterable)',
      input,
    );
  }
  if (typeof input === 'object' && input !== null && typeof input.then === 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      'a synchronous input (not Promise)',
      input,
    );
  }

  // Must be a SyncStreamable
  if (!isSyncIterable(input)) {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable', 'toStreamable'],
      input,
    );
  }

  return {
    __proto__: null,
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
  if (input == null) {
    throw new ERR_INVALID_ARG_TYPE('input', 'a non-null value', input);
  }

  // Fast path: validated source already yields valid Uint8Array[] batches
  if (input[kValidatedSource]) {
    return input;
  }

  // Check for primitives first (ByteInput)
  if (isPrimitiveChunk(input)) {
    const chunk = primitiveToUint8Array(input);
    return {
      __proto__: null,
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
        __proto__: null,
        async *[SymbolAsyncIterator]() {
          // Empty - yield nothing
        },
      };
    }
    if (isUint8Array(input[0])) {
      const allUint8 = ArrayPrototypeEvery(input, isUint8Array);
      if (allUint8) {
        const batch = input;
        return {
          __proto__: null,
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

  // Check toAsyncStreamable protocol (takes precedence over toStreamable and
  // iteration protocols)
  if (typeof input[toAsyncStreamable] === 'function') {
    const result = input[toAsyncStreamable]();
    // Synchronous validated source (e.g. Readable batched iterator)
    if (result?.[kValidatedSource]) {
      return result;
    }
    return {
      __proto__: null,
      async *[SymbolAsyncIterator]() {
        // The result may be a Promise. Check validated on both the Promise
        // itself (if tagged) and the resolved value.
        const resolved = await result;
        if (resolved?.[kValidatedSource]) {
          yield* resolved[SymbolAsyncIterator]();
          return;
        }
        yield* from(resolved)[SymbolAsyncIterator]();
      },
    };
  }

  // Check toStreamable protocol (takes precedence over iteration protocols)
  if (typeof input[toStreamable] === 'function') {
    return from(input[toStreamable]());
  }

  // Must be a Streamable (sync or async iterable)
  if (!isSyncIterable(input) && !isAsyncIterable(input)) {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable',
       'AsyncIterable', 'toStreamable', 'toAsyncStreamable'],
      input,
    );
  }

  return normalizeAsyncSource(input);
}

// =============================================================================
// Exports
// =============================================================================

module.exports = {
  from,
  fromSync,
  isAsyncIterable,
  isPrimitiveChunk,
  isSyncIterable,
  isUint8ArrayBatch,
  normalizeAsyncSource,
  normalizeAsyncValue,
  normalizeSyncSource,
  normalizeSyncValue,
  primitiveToUint8Array,
};
