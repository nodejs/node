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
  PromisePrototypeThen,
  PromiseResolve,
  PromiseWithResolvers,
  SafePromiseRace,
  Symbol,
  SymbolAsyncIterator,
  SymbolIterator,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  Uint8Array,
} = primordials;

const { markPromiseAsHandled } = internalBinding('util');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_RETURN_VALUE,
  },
} = require('internal/errors');
const { lazyDOMException } = require('internal/util');

const {
  isAnyArrayBuffer,
  isPromise,
  isTypedArray,
  isUint8Array,
} = require('internal/util/types');

const {
  kValidatedSource,
  toStreamable,
  toAsyncStreamable,
} = require('internal/streams/iter/types');

const {
  getProtocolMethod,
  toUint8Array,
} = require('internal/streams/iter/utils');

// Maximum number of chunks to yield per batch from from()/fromSync().
// Bounds peak memory when arrays flow through transforms, which must
// allocate output for the entire batch at once.
const FROM_BATCH_SIZE = 128;
const kNormalizationCancelled = Symbol('kNormalizationCancelled');

function createNormalizationContext() {
  return {
    __proto__: null,
    cancelled: false,
    reason: undefined,
    resolve: null,
    suppressCleanup: false,
  };
}

function cancelNormalization(context, reason, suppressCleanup = false) {
  if (context.cancelled) return;
  context.cancelled = true;
  context.reason = reason;
  context.suppressCleanup = suppressCleanup;
  context.resolve?.(kNormalizationCancelled);
}

function throwIfNormalizationCancelled(context) {
  if (context?.cancelled) throw context.reason;
}

async function waitForNormalization(value, context) {
  if (context === undefined) return value;
  const { promise, resolve } = PromiseWithResolvers();
  if (context.cancelled) {
    resolve(kNormalizationCancelled);
  } else {
    context.resolve = resolve;
  }
  try {
    const result = await SafePromiseRace([
      PromiseResolve(value),
      promise,
    ]);
    throwIfNormalizationCancelled(context);
    return result;
  } finally {
    if (context.resolve === resolve) context.resolve = null;
  }
}

function createNormalizationIterator(createIterator) {
  const context = createNormalizationContext();
  const iterator = createIterator(context);
  return {
    __proto__: null,
    next(value) {
      return FunctionPrototypeCall(iterator.next, iterator, value);
    },
    return(value) {
      cancelNormalization(
        context, lazyDOMException('Aborted', 'AbortError'));
      return FunctionPrototypeCall(iterator.return, iterator, value);
    },
    throw(error) {
      cancelNormalization(context, error, true);
      return FunctionPrototypeCall(iterator.throw, iterator, error);
    },
    [SymbolAsyncIterator]() {
      return this;
    },
  };
}

function createNormalizationSource(createIterator) {
  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      return createNormalizationIterator(createIterator);
    },
  };
}

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
  return arrayBufferViewToUint8Array(chunk);
}

function arrayBufferViewToUint8Array(chunk) {
  if (isTypedArray(chunk)) {
    return new Uint8Array(
      TypedArrayPrototypeGetBuffer(chunk),
      TypedArrayPrototypeGetByteOffset(chunk),
      TypedArrayPrototypeGetByteLength(chunk),
    );
  }
  return new Uint8Array(
    DataViewPrototypeGetBuffer(chunk),
    DataViewPrototypeGetByteOffset(chunk),
    DataViewPrototypeGetByteLength(chunk),
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
  const streamableMethod = getProtocolMethod(value, toStreamable);
  if (streamableMethod !== undefined) {
    const result = FunctionPrototypeCall(streamableMethod, value);
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

function* yieldBoundedBatch(batch) {
  if (batch.length === 0) {
    return;
  }
  if (batch.length <= FROM_BATCH_SIZE) {
    yield batch;
    return;
  }
  for (let i = 0; i < batch.length; i += FROM_BATCH_SIZE) {
    yield ArrayPrototypeSlice(batch, i, i + FROM_BATCH_SIZE);
  }
}

/**
 * Normalize a sync streamable source, yielding batches of Uint8Array.
 * @param {Iterable} source
 * @yields {Uint8Array[]}
 */
function* normalizeSyncSource(source) {
  let batch = [];

  for (const value of source) {
    // Fast path 1: value is already a Uint8Array[] batch
    if (isUint8ArrayBatch(value)) {
      if (batch.length > 0) {
        yield batch;
        batch = [];
      }
      yield* yieldBoundedBatch(value);
      continue;
    }
    // Fast path 2: value is a single Uint8Array (very common)
    if (isUint8Array(value)) {
      ArrayPrototypePush(batch, value);
      if (batch.length === FROM_BATCH_SIZE) {
        yield batch;
        batch = [];
      }
      continue;
    }
    // Slow path: normalize the value
    if (batch.length > 0) {
      yield batch;
      batch = [];
    }
    let valueBatch = [];
    for (const chunk of normalizeSyncValue(value)) {
      ArrayPrototypePush(valueBatch, chunk);
      if (valueBatch.length === FROM_BATCH_SIZE) {
        yield valueBatch;
        valueBatch = [];
      }
    }
    if (valueBatch.length > 0) {
      yield valueBatch;
    }
  }

  if (batch.length > 0) {
    yield batch;
  }
}

function yieldNormalizationAbortable(source, context) {
  if (context === undefined) return source;
  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      const iteratorMethod = source[SymbolAsyncIterator];
      const iterator = FunctionPrototypeCall(iteratorMethod, source);
      const nextMethod = iterator.next;
      let completed = false;
      let closed = false;
      let reading = false;

      async function closeSource(suppressError) {
        if (closed) return;
        closed = true;
        completed = true;

        if (suppressError) {
          try {
            const returnMethod = iterator.return;
            if (typeof returnMethod === 'function') {
              const cleanup = PromisePrototypeThen(
                PromiseResolve(),
                () => FunctionPrototypeCall(returnMethod, iterator));
              markPromiseAsHandled(cleanup);
            }
          } catch {
            // Cancellation has precedence over source cleanup errors.
          }
          return;
        }

        const returnMethod = iterator.return;
        if (typeof returnMethod === 'function') {
          const result = await FunctionPrototypeCall(returnMethod, iterator);
          if ((typeof result !== 'object' && typeof result !== 'function') ||
              result === null) {
            throw new ERR_INVALID_RETURN_VALUE(
              'an object', 'iterator.return()', result);
          }
        }
      }

      return {
        __proto__: null,
        async next() {
          if (completed) {
            return { __proto__: null, done: true, value: undefined };
          }
          throwIfNormalizationCancelled(context);
          reading = true;

          try {
            const next = FunctionPrototypeCall(nextMethod, iterator);
            const result = await waitForNormalization(next, context);
            if ((typeof result !== 'object' && typeof result !== 'function') ||
                result === null) {
              throw new ERR_INVALID_RETURN_VALUE(
                'an object', 'iterator.next()', result);
            }
            if (result.done) {
              reading = false;
              throwIfNormalizationCancelled(context);
              completed = true;
              closed = true;
              return { __proto__: null, done: true, value: result.value };
            }
            const value = result.value;
            reading = false;
            throwIfNormalizationCancelled(context);
            return { __proto__: null, done: false, value };
          } catch (error) {
            if (context.cancelled) await closeSource(true);
            reading = false;
            throw error;
          }
        },
        async return(value) {
          await closeSource(
            context.suppressCleanup || (context.cancelled && reading));
          return { __proto__: null, done: true, value };
        },
        async throw(error) {
          await closeSource(
            context.suppressCleanup || (context.cancelled && reading));
          throw error;
        },
        [SymbolAsyncIterator]() {
          return this;
        },
      };
    },
  };
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
async function* normalizeAsyncValue(
  value, allowNestedAsyncStreamables = true, context) {
  throwIfNormalizationCancelled(context);

  // Handle promises first
  if (isPromise(value)) {
    const resolved = await waitForNormalization(value, context);
    yield* normalizeAsyncValue(
      resolved, allowNestedAsyncStreamables, context);
    return;
  }

  // Handle primitives
  if (isPrimitiveChunk(value)) {
    yield primitiveToUint8Array(value);
    return;
  }

  const hasDisallowedAsyncIterator =
    !allowNestedAsyncStreamables && isAsyncIterable(value);
  const asyncStreamableMethod = hasDisallowedAsyncIterator ?
    undefined : getProtocolMethod(value, toAsyncStreamable);
  if (hasDisallowedAsyncIterator ||
      (!allowNestedAsyncStreamables && asyncStreamableMethod !== undefined)) {
    throw new ERR_INVALID_ARG_TYPE(
      'value',
      ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable', 'toStreamable'],
      value,
    );
  }

  // Handle ToAsyncStreamable protocol (check before ToStreamable)
  if (asyncStreamableMethod !== undefined) {
    const result = FunctionPrototypeCall(asyncStreamableMethod, value);
    if (isPromise(result)) {
      yield* normalizeAsyncValue(
        await waitForNormalization(result, context),
        allowNestedAsyncStreamables,
        context);
    } else {
      yield* normalizeAsyncValue(
        result, allowNestedAsyncStreamables, context);
    }
    return;
  }

  // Handle ToStreamable protocol
  const streamableMethod = getProtocolMethod(value, toStreamable);
  if (streamableMethod !== undefined) {
    const result = FunctionPrototypeCall(streamableMethod, value);
    yield* normalizeAsyncValue(result, allowNestedAsyncStreamables, context);
    return;
  }

  // Handle arrays (which are also iterable, but check first for efficiency)
  if (ArrayIsArray(value)) {
    for (let i = 0; i < value.length; i++) {
      yield* normalizeAsyncValue(
        value[i], allowNestedAsyncStreamables, context);
    }
    return;
  }

  // Handle async iterables (check before sync iterables since some objects
  // have both)
  if (isAsyncIterable(value)) {
    const iterable = yieldNormalizationAbortable(value, context);
    for await (const item of iterable) {
      yield* normalizeAsyncValue(item, allowNestedAsyncStreamables, context);
    }
    return;
  }

  // Handle sync iterables
  if (isSyncIterable(value)) {
    for (const item of value) {
      yield* normalizeAsyncValue(item, allowNestedAsyncStreamables, context);
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
async function* normalizeAsyncSource(source, context) {
  throwIfNormalizationCancelled(context);

  // Prefer async iteration if available
  if (isAsyncIterable(source)) {
    const iterable = yieldNormalizationAbortable(source, context);
    for await (const value of iterable) {
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
      let batch = [];
      for await (const chunk of normalizeAsyncValue(value, true, context)) {
        ArrayPrototypePush(batch, chunk);
        if (batch.length === FROM_BATCH_SIZE) {
          yield batch;
          batch = [];
        }
      }
      if (batch.length > 0) {
        yield batch;
      }
    }
    return;
  }

  // Fall back to sync iteration - batch sync values together with a bound.
  if (isSyncIterable(source)) {
    let batch = [];

    for (const value of source) {
      throwIfNormalizationCancelled(context);
      // Fast path 1: value is already a Uint8Array[] batch
      if (isUint8ArrayBatch(value)) {
        // Flush any accumulated batch first
        if (batch.length > 0) {
          yield batch;
          batch = [];
        }
        yield* yieldBoundedBatch(value);
        continue;
      }
      // Fast path 2: value is a single Uint8Array (very common)
      if (isUint8Array(value)) {
        ArrayPrototypePush(batch, value);
        if (batch.length === FROM_BATCH_SIZE) {
          yield batch;
          batch = [];
        }
        continue;
      }
      // Slow path: normalize the value - must flush and yield individually
      if (batch.length > 0) {
        yield batch;
        batch = [];
      }
      let asyncBatch = [];
      for await (const chunk of normalizeAsyncValue(value, false, context)) {
        ArrayPrototypePush(asyncBatch, chunk);
        if (asyncBatch.length === FROM_BATCH_SIZE) {
          yield asyncBatch;
          asyncBatch = [];
        }
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

async function* normalizeAsyncStreamableResult(result, context) {
  const resolved = await waitForNormalization(result, context);
  const source = resolved?.[kValidatedSource] ? resolved : from(resolved);
  yield* yieldNormalizationAbortable(source, context);
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

  // Check toStreamable protocol (takes precedence over iteration protocols).
  // toAsyncStreamable is ignored entirely in fromSync.
  const streamableMethod = getProtocolMethod(input, toStreamable);
  if (streamableMethod !== undefined) {
    return fromSync(FunctionPrototypeCall(streamableMethod, input));
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

  const isIterable = isSyncIterable(input);

  // Reject explicit async-only inputs
  if (!isIterable && isAsyncIterable(input)) {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      'a synchronous input (not AsyncIterable)',
      input,
    );
  }
  if (!isIterable &&
      typeof input === 'object' &&
      input !== null &&
      typeof input.then === 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      'a synchronous input (not Promise)',
      input,
    );
  }

  // Must be a SyncStreamable
  if (!isIterable) {
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

  // Check toAsyncStreamable protocol (takes precedence over toStreamable and
  // iteration protocols)
  const asyncStreamableMethod = getProtocolMethod(input, toAsyncStreamable);
  if (asyncStreamableMethod !== undefined) {
    let result = FunctionPrototypeCall(asyncStreamableMethod, input);
    if (isPromise(result)) {
      result = PromisePrototypeThen(result, undefined, undefined);
      markPromiseAsHandled(result);
    }
    // Synchronous validated source (e.g. Readable batched iterator)
    if (result?.[kValidatedSource]) {
      return result;
    }
    return createNormalizationSource(
      (context) => normalizeAsyncStreamableResult(result, context));
  }

  // Check toStreamable protocol (takes precedence over iteration protocols)
  const streamableMethod = getProtocolMethod(input, toStreamable);
  if (streamableMethod !== undefined) {
    return from(FunctionPrototypeCall(streamableMethod, input));
  }

  // Fast path: validated source already yields valid Uint8Array[] batches
  if (input[kValidatedSource]) {
    return input;
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

  // Must be a Streamable (sync or async iterable)
  if (!isSyncIterable(input) && !isAsyncIterable(input)) {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      ['string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable',
       'AsyncIterable', 'toStreamable', 'toAsyncStreamable'],
      input,
    );
  }

  return createNormalizationIterator(
    (context) => normalizeAsyncSource(input, context));
}

// =============================================================================
// Exports
// =============================================================================

module.exports = {
  arrayBufferViewToUint8Array,
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
