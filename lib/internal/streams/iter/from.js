'use strict';

// New Streams API - from() and fromSync()
//
// Creates normalized byte stream iterables from various input types.
// Handles recursive flattening of nested iterables and protocol conversions.

const {
  ArrayBufferIsView,
  ArrayIsArray,
  ArrayPrototypeEvery,
  ArrayPrototypeSlice,
  DataViewPrototypeGetBuffer,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetByteOffset,
  FunctionPrototypeCall,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
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
  IterResult,
  kActive,
  kDone,
  kResolvedPromise,
  kStart,
  createOperationQueue,
  getProtocolMethod,
  toUint8Array,
} = require('internal/streams/iter/utils');

// Maximum number of chunks to yield per batch from from()/fromSync().
// Bounds peak memory when arrays flow through transforms, which must
// allocate output for the entire batch at once.
const FROM_BATCH_SIZE = 128;
// Yielded by normalizeAsyncValue() (only when `emitFlush` is true) right
// before it waits on a promise or on a nested async iterable. Callers that
// batch chunks yield whatever they have collected so far, so that chunks that
// are already available are not held back until the wait completes.
const kFlushBatch = Symbol('kFlushBatch');

function createNormalizationContext() {
  return ObjectSetPrototypeOf({
    cancelled: false,
    reason: undefined,
    resolve: null,
    suppressCleanup: false,
  }, null);
}

function cancelNormalization(context, reason, suppressCleanup = false) {
  if (context.cancelled) return;
  context.cancelled = true;
  context.reason = reason;
  context.suppressCleanup = suppressCleanup;
  context.resolve?.();
}

function throwIfNormalizationCancelled(context) {
  if (context?.cancelled) throw context.reason;
}

/**
 * Wait for `value`, but stop waiting if the normalization is cancelled.
 * Settles with the first of:
 * - `value` fulfilling: its value, or the cancellation reason if the
 *   normalization has been cancelled by then;
 * - `value` rejecting: its rejection reason;
 * - cancellation: the cancellation reason.
 * This runs for every value of a normalized async source, so it uses a
 * single promise and a single reaction on `value` rather than racing
 * promises.
 * @param {any} value
 * @param {object} [context]
 * @returns {Promise<any>|any}
 */
function waitForNormalization(value, context) {
  if (context === undefined) return value;
  const { promise, resolve, reject } = PromiseWithResolvers();
  const onCancel = () => {
    if (context.resolve === onCancel) context.resolve = null;
    reject(context.reason);
  };
  PromisePrototypeThen(
    PromiseResolve(value),
    (result) => {
      if (context.resolve === onCancel) context.resolve = null;
      if (context.cancelled) {
        reject(context.reason);
      } else {
        resolve(result);
      }
    },
    (error) => {
      if (context.resolve === onCancel) context.resolve = null;
      reject(error);
    });
  if (context.cancelled) {
    // Already cancelled: a `value` that has already settled still takes
    // precedence, as its reaction above runs first.
    PromisePrototypeThen(kResolvedPromise, onCancel);
  } else {
    context.resolve = onCancel;
  }
  return promise;
}

// The method of an iterator returned by from() for a sync iterable that reads
// the next batch synchronously when it can, for pipeTo(): see nextSyncBatch()
// in createSyncSourceNormalizer().
const kNextSyncBatch = Symbol('kNextSyncBatch');

// The method of an iterator returned by from() for an async iterable that
// reads like next() when nothing can cancel the normalization while a read
// is pending, for pipeTo(): see createAsyncSourceNormalizer().
const kNextUncancellable = Symbol('kNextUncancellable');

// Methods of the iterator returned by yieldNormalizationAbortable(), used by
// createAsyncSourceNormalizer() for kNextUncancellable.
const kReadUncancellable = Symbol('kReadUncancellable');
const kToIterResult = Symbol('kToIterResult');
const kReadFailed = Symbol('kReadFailed');
// The result kReadUncancellable gives once the source is done.
const kDoneSourceResult = ObjectFreeze({ __proto__: null, done: true, value: undefined });

function createNormalizationIterator(createIterator) {
  const context = createNormalizationContext();
  const iterator = createIterator(context);
  if (iterator[kNextUncancellable] !== undefined) {
    return ObjectSetPrototypeOf({
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
      [kNextUncancellable]() {
        return iterator[kNextUncancellable]();
      },
      [SymbolAsyncIterator]() {
        return this;
      },
      [kValidatedSource]: true,
    }, null);
  }
  if (iterator[kNextSyncBatch] !== undefined) {
    return ObjectSetPrototypeOf({
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
      [kNextSyncBatch]() {
        return iterator[kNextSyncBatch]();
      },
      [SymbolAsyncIterator]() {
        return this;
      },
      [kValidatedSource]: true,
    }, null);
  }
  return ObjectSetPrototypeOf({
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
    [kValidatedSource]: true,
  }, null);
}

function createNormalizationSource(createIterator) {
  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      return createNormalizationIterator(createIterator);
    },
    [kValidatedSource]: true,
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
      if (value.length <= FROM_BATCH_SIZE) {
        if (value.length !== 0) yield value;
      } else {
        yield* yieldBoundedBatch(value);
      }
      continue;
    }
    // Fast path 2: value is a single Uint8Array (very common)
    if (isUint8Array(value)) {
      batch[batch.length] = value;
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
      valueBatch[valueBatch.length] = chunk;
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

      // Settle a pending next() with a result of the source's next(). Throws
      // (to be handled by the caller) like the checks it replaces would.
      function toIterResult(result) {
        throwIfNormalizationCancelled(context);
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
          return new IterResult(true, result.value);
        }
        const value = result.value;
        reading = false;
        throwIfNormalizationCancelled(context);
        return new IterResult(false, value);
      }

      function onUncancellableResult(result) {
        try {
          return toIterResult(result);
        } catch (error) {
          reading = false;
          throw error;
        }
      }

      // Reject a pending next(), closing the source first if the
      // normalization has been cancelled.
      function rejectNext(reject, error) {
        if (context.cancelled) {
          PromisePrototypeThen(closeSource(true), () => {
            reading = false;
            reject(error);
          });
          return;
        }
        reading = false;
        reject(error);
      }

      return ObjectSetPrototypeOf({
        // next() runs for every value of the source, so instead of being an
        // async function awaiting waitForNormalization(), it waits for the
        // source with a single promise, which a cancellation rejects
        // directly.
        next() {
          if (completed) {
            return PromiseResolve(new IterResult(true, undefined));
          }
          if (context.cancelled) return PromiseReject(context.reason);
          reading = true;

          const { promise, resolve, reject } = PromiseWithResolvers();
          let next;
          try {
            next = FunctionPrototypeCall(nextMethod, iterator);
          } catch (error) {
            rejectNext(reject, error);
            return promise;
          }
          // The first of the source's result and a cancellation settles
          // next(); whichever comes later is ignored.
          let settled = false;
          const onCancel = () => {
            if (settled) return;
            settled = true;
            if (context.resolve === onCancel) context.resolve = null;
            rejectNext(reject, context.reason);
          };
          context.resolve = onCancel;
          PromisePrototypeThen(
            PromiseResolve(next),
            (result) => {
              if (settled) return;
              settled = true;
              if (context.resolve === onCancel) context.resolve = null;
              let iterResult;
              try {
                iterResult = toIterResult(result);
              } catch (error) {
                rejectNext(reject, error);
                return;
              }
              resolve(iterResult);
            },
            (error) => {
              if (settled) return;
              settled = true;
              if (context.resolve === onCancel) context.resolve = null;
              rejectNext(reject, error);
            });
          return promise;
        },
        // Like next(), when nothing cancels the normalization while the read
        // is pending, in two parts so that the caller handles the result in
        // the same reaction: kReadUncancellable starts the read, returning a
        // promise for the source's result, and the caller passes that result
        // to kToIterResult, or calls kReadFailed if the promise rejects.
        [kReadUncancellable]() {
          if (completed) {
            return PromiseResolve(kDoneSourceResult);
          }
          if (context.cancelled) return PromiseReject(context.reason);
          reading = true;
          let next;
          try {
            next = FunctionPrototypeCall(nextMethod, iterator);
          } catch (error) {
            reading = false;
            return PromiseReject(error);
          }
          return PromiseResolve(next);
        },
        [kToIterResult]: onUncancellableResult,
        [kReadFailed]() {
          reading = false;
        },
        async return(value) {
          await closeSource(
            context.suppressCleanup || (context.cancelled && reading));
          return new IterResult(true, value);
        },
        async throw(error) {
          await closeSource(
            context.suppressCleanup || (context.cancelled && reading));
          throw error;
        },
        [SymbolAsyncIterator]() {
          return this;
        },
      }, null);
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
  value, allowNestedAsyncStreamables = true, context, emitFlush = false) {
  throwIfNormalizationCancelled(context);

  // Handle promises first
  if (isPromise(value)) {
    if (emitFlush) yield kFlushBatch;
    const resolved = await waitForNormalization(value, context);
    yield* normalizeAsyncValue(
      resolved, allowNestedAsyncStreamables, context, emitFlush);
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
      if (emitFlush) yield kFlushBatch;
      yield* normalizeAsyncValue(
        await waitForNormalization(result, context),
        allowNestedAsyncStreamables,
        context,
        emitFlush);
    } else {
      yield* normalizeAsyncValue(
        result, allowNestedAsyncStreamables, context, emitFlush);
    }
    return;
  }

  // Handle ToStreamable protocol
  const streamableMethod = getProtocolMethod(value, toStreamable);
  if (streamableMethod !== undefined) {
    const result = FunctionPrototypeCall(streamableMethod, value);
    yield* normalizeAsyncValue(
      result, allowNestedAsyncStreamables, context, emitFlush);
    return;
  }

  // Handle arrays (which are also iterable, but check first for efficiency)
  if (ArrayIsArray(value)) {
    for (let i = 0; i < value.length; i++) {
      yield* normalizeAsyncValue(
        value[i], allowNestedAsyncStreamables, context, emitFlush);
    }
    return;
  }

  // Handle async iterables (check before sync iterables since some objects
  // have both)
  if (isAsyncIterable(value)) {
    const iterable = yieldNormalizationAbortable(value, context);
    if (emitFlush) yield kFlushBatch;
    for await (const item of iterable) {
      yield* normalizeAsyncValue(
        item, allowNestedAsyncStreamables, context, emitFlush);
      if (emitFlush) yield kFlushBatch;
    }
    return;
  }

  // Handle sync iterables
  if (isSyncIterable(value)) {
    for (const item of value) {
      yield* normalizeAsyncValue(
        item, allowNestedAsyncStreamables, context, emitFlush);
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
 * Normalize a value of an async source that is neither a Uint8Array nor a
 * Uint8Array[] batch, yielding batches of Uint8Array. Chunks are batched,
 * but whatever has been collected is yielded before waiting on async content
 * (such as a nested async iterable), so data is not held back until it ends.
 * @param {any} value
 * @param {object} context
 * @param {boolean} [allowNestedAsync] Whether nested async iterables and
 *   toAsyncStreamable are allowed (they are not in sync sources).
 * @yields {Uint8Array[]}
 */
async function* normalizeAsyncSourceValue(value, context,
                                          allowNestedAsync = true) {
  let batch = [];
  for await (const chunk of
    normalizeAsyncValue(value, allowNestedAsync, context, true)) {
    if (chunk === kFlushBatch) {
      if (batch.length > 0) {
        yield batch;
        batch = [];
      }
      continue;
    }
    batch[batch.length] = chunk;
    if (batch.length === FROM_BATCH_SIZE) {
      yield batch;
      batch = [];
    }
  }
  if (batch.length > 0) {
    yield batch;
  }
}

/**
 * Iterator normalizing an async iterable source into batches of Uint8Array.
 *
 * This is what an async generator looping over the source with for await
 * would do, written out by hand: the source is read for every batch, and an
 * async generator layer costs several promises and an async frame each
 * time. Values that are already batches or Uint8Arrays (the common case)
 * are handled with one promise per batch; any other value is normalized by
 * normalizeAsyncSourceValue(). As with an async generator:
 * - nothing happens until the first next();
 * - calls made while one is in progress are queued;
 * - an error from the source ends the iteration without closing the source,
 *   and an error normalizing a value closes the source first;
 * - return() closes the value being normalized and the source, propagating
 *   errors from closing them, and throw() closes them ignoring such errors.
 * @param {AsyncIterable} source
 * @param {object} context
 * @returns {object} An object with next(), return() and throw().
 */
function createAsyncSourceNormalizer(source, context) {
  let state = kStart;
  // The source iterator, from yieldNormalizationAbortable(). Its return()
  // is an async function, so calling it cannot throw synchronously.
  let iterator;
  // The value being normalized: a sync iterator of the sub-batches of an
  // oversized batch, or a normalizeAsyncSourceValue() generator.
  let boundedBatches = null;
  let valueBatches = null;
  // Whether the source is read with kNextUncancellable: set by the
  // kNextUncancellable method, for callers that never cancel the
  // normalization while a read is pending.
  let uncancellable = false;
  const { run, settled } = createOperationQueue();

  function finish(result) {
    settled();
    return result;
  }

  function fail(error) {
    state = kDone;
    settled();
    throw error;
  }

  // Closing for a throw completion: wait, but ignore errors.
  function closeQuietly(it) {
    return PromisePrototypeThen(it.return(), undefined, () => {});
  }

  function onBodyError(error) {
    // Like for await when its body throws: close the source, keep the error.
    valueBatches = null;
    boundedBatches = null;
    state = kDone;
    return PromisePrototypeThen(
      closeQuietly(iterator), () => fail(error));
  }

  function onSourceResult(result) {
    if (result.done) {
      state = kDone;
      return finish(new IterResult(true, undefined));
    }
    try {
      return handleValue(result.value);
    } catch (error) {
      return onBodyError(error);
    }
  }

  function handleValue(value) {
    if (isUint8ArrayBatch(value)) {
      if (value.length <= FROM_BATCH_SIZE) {
        if (value.length === 0) return pullSource();
        return finish(new IterResult(false, value));
      }
      boundedBatches = yieldBoundedBatch(value);
      return pullValue();
    }
    if (isUint8Array(value)) return finish(new IterResult(false, [value]));
    valueBatches = normalizeAsyncSourceValue(value, context);
    return pullValue();
  }

  function onValueResult(result) {
    if (result.done) {
      valueBatches = null;
      return pullSource();
    }
    return finish(new IterResult(false, result.value));
  }

  // A kReadUncancellable result: handled as the result of the source's
  // next() is, in the same reaction.
  function onUncancellableResult(result) {
    let iterResult;
    try {
      iterResult = iterator[kToIterResult](result);
    } catch (error) {
      return fail(error);
    }
    return onSourceResult(iterResult);
  }

  function onUncancellableError(error) {
    iterator[kReadFailed]();
    return fail(error);
  }

  function pullSource() {
    if (uncancellable) {
      return PromisePrototypeThen(iterator[kReadUncancellable](),
                                  onUncancellableResult, onUncancellableError);
    }
    return PromisePrototypeThen(iterator.next(), onSourceResult, fail);
  }

  function pullValue() {
    if (boundedBatches !== null) {
      const result = boundedBatches.next();
      if (!result.done) return finish(new IterResult(false, result.value));
      boundedBatches = null;
      return pullSource();
    }
    return PromisePrototypeThen(
      valueBatches.next(), onValueResult, onBodyError);
  }

  function doNext() {
    if (state === kDone) {
      return PromiseResolve(finish(new IterResult(true, undefined)));
    }
    if (state === kStart) {
      try {
        throwIfNormalizationCancelled(context);
        const iterable = yieldNormalizationAbortable(source, context);
        iterator = iterable[SymbolAsyncIterator]();
      } catch (error) {
        state = kDone;
        settled();
        return PromiseReject(error);
      }
      state = kActive;
    }
    if (boundedBatches !== null || valueBatches !== null) {
      return PromiseResolve(pullValue());
    }
    return pullSource();
  }

  function doReturn(value) {
    const result = new IterResult(true, value);
    if (state !== kActive) {
      state = kDone;
      return PromiseResolve(finish(result));
    }
    state = kDone;
    boundedBatches = null;
    const pending = valueBatches;
    valueBatches = null;
    // Close the value being normalized, then the source. If closing the
    // value fails, the source is still closed and the error kept.
    let closed;
    if (pending === null) {
      closed = iterator.return();
    } else {
      closed = PromisePrototypeThen(
        pending.return(),
        () => iterator.return(),
        (error) => PromisePrototypeThen(closeQuietly(iterator), () => {
          throw error;
        }));
    }
    return PromisePrototypeThen(closed, () => finish(result), fail);
  }

  function doThrow(error) {
    if (state !== kActive) {
      state = kDone;
      settled();
      return PromiseReject(error);
    }
    state = kDone;
    boundedBatches = null;
    const pending = valueBatches;
    valueBatches = null;
    const closed = pending === null ? closeQuietly(iterator) :
      PromisePrototypeThen(closeQuietly(pending), () => closeQuietly(iterator));
    return PromisePrototypeThen(closed, () => fail(error));
  }

  return ObjectSetPrototypeOf({
    next() { return run(doNext); },
    return(value) { return run(doReturn, value); },
    throw(error) { return run(doThrow, error); },
    [kNextUncancellable]() {
      uncancellable = true;
      return run(doNext);
    },
  }, null);
}

/**
 * Normalize an async streamable source, yielding batches of Uint8Array.
 * @param {AsyncIterable|Iterable} source
 * @param {object} context
 * @returns {object} An async iterator.
 */
function normalizeAsyncSource(source, context) {
  // Prefer async iteration if available.
  if (isAsyncIterable(source)) {
    return createAsyncSourceNormalizer(source, context);
  }
  return createSyncSourceNormalizer(source, context);
}

// A value of a sync source that is not a Uint8Array or a Uint8Array[]
// batch, yielded by readSyncSource() to be normalized asynchronously.
function SyncSourceValue(value) {
  this.value = value;
}
SyncSourceValue.prototype = ObjectFreeze({ __proto__: null });

/**
 * Read a sync iterable source for from(): yields Uint8Array[] batches,
 * collecting single Uint8Arrays into batches of up to FROM_BATCH_SIZE
 * chunks, and a SyncSourceValue for any other value, after the chunks
 * collected before it.
 * @param {Iterable} source
 * @param {object} context
 * @yields {Uint8Array[]|SyncSourceValue}
 */
function* readSyncSource(source, context) {
  throwIfNormalizationCancelled(context);
  if (!isSyncIterable(source)) {
    throw new ERR_INVALID_ARG_TYPE(
      'source', ['Iterable', 'AsyncIterable'], source);
  }
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
      if (value.length <= FROM_BATCH_SIZE) {
        if (value.length !== 0) yield value;
      } else {
        yield* yieldBoundedBatch(value);
      }
      continue;
    }
    // Fast path 2: value is a single Uint8Array (very common)
    if (isUint8Array(value)) {
      batch[batch.length] = value;
      if (batch.length === FROM_BATCH_SIZE) {
        yield batch;
        batch = [];
      }
      continue;
    }
    // Slow path: flush, then have the value normalized
    if (batch.length > 0) {
      yield batch;
      batch = [];
    }
    yield new SyncSourceValue(value);
  }
  // Yield any remaining batched values
  if (batch.length > 0) {
    yield batch;
  }
}

/**
 * Iterator normalizing a sync iterable source for from() into batches of
 * Uint8Array, without an async generator layer for every batch (see
 * createAsyncSourceNormalizer()).
 *
 * The source is read by the sync generator readSyncSource(), so for...of
 * reads and closes it, and this iterator only adds the asynchronous
 * normalization of values that need it. Operations on the generator mirror
 * those on the async generator this replaces: return() and throw() are
 * passed to it, after closing the value being normalized, and an error
 * normalizing a value is thrown into it, so that for...of closes the source
 * as for an error in the loop body.
 * @param {Iterable} source
 * @param {object} context
 * @returns {object} An object with next(), return() and throw().
 */
function createSyncSourceNormalizer(source, context) {
  const reader = readSyncSource(source, context);
  let done = false;
  // A normalizeAsyncSourceValue() generator for the value being normalized.
  let valueBatches = null;
  const { run, settled, release, idle } = createOperationQueue();

  // Results are often produced synchronously. An async generator stays busy
  // until the tick after a yield or return (both await their operand), so
  // that a next(), return() or throw() made synchronously after this one is
  // queued behind it; do the same.
  function finish(result) {
    PromisePrototypeThen(kResolvedPromise, release);
    return result;
  }

  function fail(error) {
    done = true;
    settled();
    throw error;
  }

  // Throw `error` into the reader, closing the source as for...of does when
  // its body throws: errors from closing it are ignored.
  function throwIntoReader(error) {
    try {
      reader.throw(error);
    } catch {
      // The reader rethrows `error`.
    }
  }

  function onValueResult(result) {
    if (result.done) {
      valueBatches = null;
      return produce();
    }
    return finish(new IterResult(false, result.value));
  }

  function onValueError(error) {
    valueBatches = null;
    throwIntoReader(error);
    return fail(error);
  }

  // Produce the next result: an IterResult, or a promise for one when a
  // value is normalized asynchronously. Throws, after settling, on error.
  function produce() {
    if (valueBatches !== null) {
      return PromisePrototypeThen(
        valueBatches.next(), onValueResult, onValueError);
    }
    let result;
    try {
      result = reader.next();
    } catch (error) {
      return fail(error);
    }
    if (result.done) {
      done = true;
      return finish(new IterResult(true, undefined));
    }
    const value = result.value;
    if (ArrayIsArray(value)) return finish(new IterResult(false, value));
    valueBatches = normalizeAsyncSourceValue(value.value, context, false);
    return PromisePrototypeThen(
      valueBatches.next(), onValueResult, onValueError);
  }

  function doNext() {
    if (done) return PromiseResolve(finish(new IterResult(true, undefined)));
    try {
      return PromiseResolve(produce());
    } catch (error) {
      return PromiseReject(error);
    }
  }

  function doReturn(value) {
    const result = new IterResult(true, value);
    if (done) return PromiseResolve(finish(result));
    done = true;
    const pending = valueBatches;
    valueBatches = null;
    if (pending === null) {
      try {
        reader.return();
      } catch (error) {
        settled();
        return PromiseReject(error);
      }
      return PromiseResolve(finish(result));
    }
    // Close the value being normalized, then the source. If closing the
    // value fails, the source is still closed and the error kept.
    return PromisePrototypeThen(pending.return(), () => {
      try {
        reader.return();
      } catch (error) {
        return fail(error);
      }
      return finish(result);
    }, (error) => {
      throwIntoReader(error);
      return fail(error);
    });
  }

  function doThrow(error) {
    if (done) {
      settled();
      return PromiseReject(error);
    }
    done = true;
    const pending = valueBatches;
    valueBatches = null;
    if (pending === null) {
      throwIntoReader(error);
      settled();
      return PromiseReject(error);
    }
    return PromisePrototypeThen(
      PromisePrototypeThen(pending.return(), undefined, () => {}),
      () => {
        throwIntoReader(error);
        return fail(error);
      });
  }

  // Read the next batch like next() but synchronously, when it is read
  // synchronously from the source and no operation is running or queued:
  // returns the batch, or null when done. Returns undefined, having started
  // nothing but the normalization of a value, when next() must be used.
  // Throws on error, like next() rejects.
  function nextSyncBatch() {
    if (!idle() || valueBatches !== null) return undefined;
    if (done) return null;
    let result;
    try {
      result = reader.next();
    } catch (error) {
      done = true;
      throw error;
    }
    if (result.done) {
      done = true;
      return null;
    }
    const value = result.value;
    if (ArrayIsArray(value)) return value;
    valueBatches = normalizeAsyncSourceValue(value.value, context, false);
    return undefined;
  }

  return ObjectSetPrototypeOf({
    next() { return run(doNext); },
    return(value) { return run(doReturn, value); },
    throw(error) { return run(doThrow, error); },
    [kNextSyncBatch]: nextSyncBatch,
  }, null);
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
      'a synchronous input, not an async iterable',
      input,
    );
  }
  if (!isIterable &&
      typeof input === 'object' &&
      input !== null &&
      typeof input.then === 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'input',
      'a synchronous input, not a promise',
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
      [kValidatedSource]: true,
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
        [kValidatedSource]: true,
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
          [kValidatedSource]: true,
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
  kNextSyncBatch,
  kNextUncancellable,
  normalizeAsyncSource,
  normalizeAsyncValue,
  normalizeSyncSource,
  normalizeSyncValue,
  primitiveToUint8Array,
};
