'use strict';

// New Streams API - Pull Pipeline
//
// pull(), pullSync(), pipeTo(), pipeToSync()
// Pull-through pipelines with transforms. Data flows on-demand from source
// through transforms to consumer.

const {
  ArrayBufferIsView,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  FunctionPrototypeCall,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  Symbol,
  SymbolAsyncIterator,
  SymbolIterator,
  TypedArrayPrototypeGetByteLength,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_RETURN_VALUE,
    ERR_INVALID_STATE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { lazyDOMException } = require('internal/util');
const {
  isAnyArrayBuffer,
  isPromise,
  isUint8Array,
} = require('internal/util/types');
const {
  AbortController,
  AbortSignal,
  abortSignal,
} = require('internal/abort_controller');

const {
  arrayBufferViewToUint8Array,
  from,
  fromSync,
  isSyncIterable,
  isAsyncIterable,
  isUint8ArrayBatch,
} = require('internal/streams/iter/from');

const {
  IterResult,
  kActive,
  kDone,
  kNullOnceOption,
  kResolvedPromise,
  kStart,
  callWithByteView,
  createBatchEntry,
  createOperationQueue,
  isTransformObject,
  parsePullArgs,
  snapshotTransform,
  toUint8Array,
  validateBatchEntry,
  validateByteView,
  yieldAbortable,
} = require('internal/streams/iter/utils');
const {
  converters,
} = require('internal/streams/iter/webidl');

const {
  kValidatedTransform,
} = require('internal/streams/iter/types');

const { markPromiseAsHandled } = internalBinding('util');

// =============================================================================
// Type Guards and Helpers
// =============================================================================

/**
 * Check if a value is a Writer (has write method).
 * @returns {boolean}
 */
function hasMethod(value, name) {
  return typeof value?.[name] === 'function';
}

/**
 * Parse pipeTo/pipeToSync arguments: [...transforms, writer, options?]
 * @param {Array} args
 * @param {string} requiredMethod - 'write' for pipeTo, 'writeSync' for pipeToSync
 * @returns {{ transforms: Array, writer: object, options: unknown }}
 */
function parsePipeToArgs(args, requiredMethod) {
  if (args.length === 0) {
    throw new ERR_INVALID_ARG_VALUE('args', args, 'pipeTo requires a writer argument');
  }

  let options;
  let writerIndex = args.length - 1;

  // Check if last arg is options
  const last = args[args.length - 1];
  if (snapshotTransform(last) === undefined &&
      !hasMethod(last, requiredMethod)) {
    options = last;
    writerIndex = args.length - 2;
  }

  if (writerIndex < 0) {
    throw new ERR_INVALID_ARG_VALUE('args', args, 'pipeTo requires a writer argument');
  }

  const writer = args[writerIndex];
  if (!hasMethod(writer, requiredMethod)) {
    throw new ERR_INVALID_ARG_TYPE(
      'writer', `object with a ${requiredMethod} method`, writer);
  }

  const transforms = ArrayPrototypeSlice(args, 0, writerIndex);
  for (let i = 0; i < transforms.length; i++) {
    const transform = snapshotTransform(transforms[i]);
    if (transform === undefined) {
      throw new ERR_INVALID_ARG_TYPE(
        `transforms[${i}]`, ['Function', 'Object with transform()'],
        transforms[i]);
    }
    transforms[i] = transform;
  }

  return {
    __proto__: null,
    transforms,
    writer,
    options,
  };
}

// =============================================================================
// Transform Output Flattening
// =============================================================================

/**
 * Flatten transform yield to Uint8Array chunks (sync).
 * @yields {Uint8Array}
 */
function* flattenTransformYieldSync(value) {
  if (isUint8Array(value)) {
    yield value;
    return;
  }
  if (typeof value === 'string') {
    yield toUint8Array(value);
    return;
  }
  if (isAnyArrayBuffer(value)) {
    yield new Uint8Array(value);
    return;
  }
  if (ArrayBufferIsView(value)) {
    yield arrayBufferViewToUint8Array(value);
    return;
  }
  // Must be Iterable<TransformYield>
  if (isSyncIterable(value)) {
    for (const item of value) {
      yield* flattenTransformYieldSync(item);
    }
    return;
  }
  throw new ERR_INVALID_ARG_TYPE(
    'value',
    ['Uint8Array', 'string', 'ArrayBuffer', 'ArrayBufferView', 'Iterable'],
    value);
}

/**
 * Flatten transform yield to Uint8Array chunks (async).
 * @yields {Uint8Array}
 */
async function* flattenTransformYieldAsync(value) {
  if (isUint8Array(value)) {
    yield value;
    return;
  }
  if (typeof value === 'string') {
    yield toUint8Array(value);
    return;
  }
  if (isAnyArrayBuffer(value)) {
    yield new Uint8Array(value);
    return;
  }
  if (ArrayBufferIsView(value)) {
    yield arrayBufferViewToUint8Array(value);
    return;
  }
  // Check for async iterable first
  if (isAsyncIterable(value)) {
    for await (const item of value) {
      yield* flattenTransformYieldAsync(item);
    }
    return;
  }
  // Must be sync Iterable<TransformYield>, no nested async iterables
  if (isSyncIterable(value)) {
    for (const item of value) {
      yield* flattenTransformYieldSync(item);
    }
    return;
  }
  throw new ERR_INVALID_ARG_TYPE(
    'value',
    ['Uint8Array', 'string', 'ArrayBuffer', 'ArrayBufferView',
     'Iterable', 'AsyncIterable'],
    value);
}

/**
 * Process transform result (sync).
 * @yields {Uint8Array[]}
 */
function* processTransformResultSync(result) {
  if (result === null) {
    return;
  }
  // Single Uint8Array -> wrap as batch
  if (isUint8Array(result)) {
    yield [result];
    return;
  }
  // String -> UTF-8 encode and wrap as batch
  if (typeof result === 'string') {
    yield [toUint8Array(result)];
    return;
  }
  // ArrayBuffer / ArrayBufferView -> convert and wrap
  if (isAnyArrayBuffer(result)) {
    yield [new Uint8Array(result)];
    return;
  }
  if (ArrayBufferIsView(result)) {
    yield [arrayBufferViewToUint8Array(result)];
    return;
  }
  // Uint8Array[] batch
  if (isUint8ArrayBatch(result)) {
    if (result.length > 0) {
      yield result;
    }
    return;
  }
  // Iterable or Generator
  if (isSyncIterable(result)) {
    const batch = [];
    for (const item of result) {
      for (const chunk of flattenTransformYieldSync(item)) {
        ArrayPrototypePush(batch, chunk);
      }
    }
    if (batch.length > 0) {
      yield batch;
    }
    return;
  }
  throw new ERR_INVALID_ARG_TYPE(
    'result',
    ['null', 'Uint8Array', 'string', 'ArrayBuffer',
     'ArrayBufferView', 'Array', 'Iterable'],
    result);
}

/**
 * Append normalized transform result batches to an array (sync).
 * @param {Array<Uint8Array[]>} target
 * @param {*} result
 */
function appendTransformResultSync(target, result) {
  if (result === null) {
    return;
  }
  if (isUint8ArrayBatch(result)) {
    if (result.length > 0) {
      ArrayPrototypePush(target, result);
    }
    return;
  }
  if (isUint8Array(result)) {
    ArrayPrototypePush(target, [result]);
    return;
  }
  if (typeof result === 'string') {
    ArrayPrototypePush(target, [toUint8Array(result)]);
    return;
  }
  if (isAnyArrayBuffer(result)) {
    ArrayPrototypePush(target, [new Uint8Array(result)]);
    return;
  }
  if (ArrayBufferIsView(result)) {
    ArrayPrototypePush(target, [arrayBufferViewToUint8Array(result)]);
    return;
  }
  for (const batch of processTransformResultSync(result)) {
    ArrayPrototypePush(target, batch);
  }
}

/**
 * Process transform result (async).
 * @yields {Uint8Array[]}
 */
async function* processTransformResultAsync(result) {
  // Handle Promise
  if (isPromise(result)) {
    const resolved = await result;
    yield* processTransformResultAsync(resolved);
    return;
  }
  if (result === null) {
    return;
  }
  // Single Uint8Array -> wrap as batch
  if (isUint8Array(result)) {
    yield [result];
    return;
  }
  // String -> UTF-8 encode and wrap as batch
  if (typeof result === 'string') {
    yield [toUint8Array(result)];
    return;
  }
  // ArrayBuffer / ArrayBufferView -> convert and wrap
  if (isAnyArrayBuffer(result)) {
    yield [new Uint8Array(result)];
    return;
  }
  if (ArrayBufferIsView(result)) {
    yield [arrayBufferViewToUint8Array(result)];
    return;
  }
  // Uint8Array[] batch
  if (isUint8ArrayBatch(result)) {
    if (result.length > 0) {
      yield result;
    }
    return;
  }
  // Check for async iterable/generator first
  if (isAsyncIterable(result)) {
    const batch = [];
    for await (const item of result) {
      if (isUint8Array(item)) {
        ArrayPrototypePush(batch, item);
        continue;
      }
      for await (const chunk of flattenTransformYieldAsync(item)) {
        ArrayPrototypePush(batch, chunk);
      }
    }
    if (batch.length > 0) {
      yield batch;
    }
    return;
  }
  // Sync Iterable or Generator
  if (isSyncIterable(result)) {
    const batch = [];
    for (const item of result) {
      if (isUint8Array(item)) {
        ArrayPrototypePush(batch, item);
        continue;
      }
      // Note: This iteration is synchronous, since async iterables
      // may not be nested within sync iterables.
      for (const chunk of flattenTransformYieldSync(item)) {
        ArrayPrototypePush(batch, chunk);
      }
    }
    if (batch.length > 0) {
      yield batch;
    }
    return;
  }
  throw new ERR_INVALID_ARG_TYPE(
    'result',
    ['null', 'Uint8Array', 'string', 'ArrayBuffer',
     'ArrayBufferView', 'Array', 'Iterable', 'AsyncIterable', 'Promise'],
    result);
}

/**
 * Append normalized transform result batches to an array (async).
 * @param {Array<Uint8Array[]>} target
 * @param {*} result
 * @returns {Promise<void>|undefined}
 */
function appendTransformResultAsync(target, result) {
  if (result === null) {
    return;
  }
  if (isUint8ArrayBatch(result)) {
    if (result.length > 0) {
      ArrayPrototypePush(target, result);
    }
    return;
  }
  if (isUint8Array(result)) {
    ArrayPrototypePush(target, [result]);
    return;
  }
  if (typeof result === 'string') {
    ArrayPrototypePush(target, [toUint8Array(result)]);
    return;
  }
  if (isAnyArrayBuffer(result)) {
    ArrayPrototypePush(target, [new Uint8Array(result)]);
    return;
  }
  if (ArrayBufferIsView(result)) {
    ArrayPrototypePush(target, [arrayBufferViewToUint8Array(result)]);
    return;
  }
  return appendTransformResultAsyncSlow(target, result);
}

async function appendTransformResultAsyncSlow(target, result) {
  for await (const batch of processTransformResultAsync(result)) {
    ArrayPrototypePush(target, batch);
  }
}

function normalizeTransformResultFast(result) {
  if (isUint8ArrayBatch(result)) {
    return result.length === 0 ? null : result;
  }
  if (isUint8Array(result)) return [result];
  if (typeof result === 'string') return [toUint8Array(result)];
  if (isAnyArrayBuffer(result)) return [new Uint8Array(result)];
  if (ArrayBufferIsView(result)) return [arrayBufferViewToUint8Array(result)];
}

// =============================================================================
// Sync Pipeline Implementation
// =============================================================================

/**
 * Apply a single stateless sync transform to a source.
 * @yields {Uint8Array[]}
 */
/**
 * Apply a fused run of stateless sync transforms.
 * @param {Iterable<Uint8Array[]>} source
 * @param {Array<Function>} run - Array of stateless transform functions
 * @yields {Uint8Array[]}
 */
function* applyFusedStatelessSyncTransforms(source, run) {
  for (const chunks of source) {
    let current = chunks;
    for (let i = 0; i < run.length; i++) {
      const result = run[i](current);
      if (result === null) {
        current = null;
        break;
      }
      if (i === run.length - 1) {
        current = result;
        continue;
      }
      current = normalizeTransformResultFast(result);
      if (current === undefined) {
        const normalized = [];
        appendTransformResultSync(normalized, result);
        current = normalized.length === 0 ? null : normalized[0];
      }
      if (current === null) break;
    }
    if (current === null) continue;
    // Inline normalization with Uint8Array[] batch as the fast path,
    // matching the async pipeline's check order.
    if (isUint8ArrayBatch(current)) {
      if (current.length > 0) yield current;
    } else if (isUint8Array(current)) {
      yield [current];
    } else if (typeof current === 'string') {
      yield [toUint8Array(current)];
    } else if (isAnyArrayBuffer(current)) {
      yield [new Uint8Array(current)];
    } else if (ArrayBufferIsView(current)) {
      yield [arrayBufferViewToUint8Array(current)];
    } else {
      yield* processTransformResultSync(current);
    }
  }
  // Flush each transform after all upstream data, including data emitted by
  // earlier flushes, has been processed by that transform.
  let pending = [];
  for (let i = 0; i < run.length; i++) {
    const next = [];
    for (let j = 0; j < pending.length; j++) {
      appendTransformResultSync(next, run[i](pending[j]));
    }
    appendTransformResultSync(next, run[i](null));
    pending = next;
  }
  for (let i = 0; i < pending.length; i++) {
    yield pending[i];
  }
}

/**
 * Apply a single stateful sync transform to a source.
 * @yields {Uint8Array[]}
 */
function* withFlushSync(source) {
  yield* source;
  yield null;
}

function* applyStatefulSyncTransform(source, transform, receiver) {
  const output = FunctionPrototypeCall(
    transform, receiver, withFlushSync(source));
  for (const item of output) {
    if (item === null) continue;
    const batch = [];
    for (const chunk of flattenTransformYieldSync(item)) {
      ArrayPrototypePush(batch, chunk);
    }
    if (batch.length > 0) {
      yield batch;
    }
  }
}

/**
 * Create a sync pipeline from source through transforms.
 * @yields {Uint8Array[]}
 */
function* createSyncPipeline(source, transforms) {
  let current = source;

  // Apply transforms - fuse consecutive stateless transforms into a single
  // generator layer to avoid unnecessary generator ticks.
  let statelessRun = [];

  for (let i = 0; i < transforms.length; i++) {
    const transform = transforms[i];
    if (isTransformObject(transform)) {
      if (statelessRun.length > 0) {
        current = applyFusedStatelessSyncTransforms(current, statelessRun);
        statelessRun = [];
      }
      current = applyStatefulSyncTransform(
        current, transform.transform, transform.receiver);
    } else {
      ArrayPrototypePush(statelessRun, transform);
    }
  }
  if (statelessRun.length > 0) {
    current = applyFusedStatelessSyncTransforms(current, statelessRun);
  }

  yield* current;
}

// =============================================================================
// Async Pipeline Implementation
// =============================================================================

// The options object passed to async transforms. Stateless transforms get a
// new one for every call, so it is constructed rather than created as a
// `{ __proto__: null, signal }` literal (a dictionary-mode object). The
// prototype is frozen and has no %Object.prototype% in its chain, and the
// non-enumerable `constructor` lets util.inspect() print the options as
// `TransformOptions { signal }`.
function TransformOptions(signal) {
  this.signal = signal;
}
TransformOptions.prototype = ObjectFreeze(ObjectDefineProperty(
  { __proto__: null }, 'constructor',
  { __proto__: null, value: TransformOptions }));

/**
 * Close an async iterator as for await does: for a throw completion
 * (`quiet`), wait for it but ignore errors; otherwise reject on errors and
 * on a result that is not an object.
 * @param {object} iterator
 * @param {boolean} quiet
 * @returns {Promise<void>}
 */
function closeAsyncIterator(iterator, quiet) {
  let promise;
  try {
    const returnMethod = iterator.return;
    if (returnMethod === undefined || returnMethod === null) {
      return kResolvedPromise;
    }
    promise = PromiseResolve(FunctionPrototypeCall(returnMethod, iterator));
  } catch (error) {
    return quiet ? kResolvedPromise : PromiseReject(error);
  }
  if (quiet) return PromisePrototypeThen(promise, undefined, () => {});
  return PromisePrototypeThen(promise, (result) => {
    if ((typeof result !== 'object' && typeof result !== 'function') ||
        result === null) {
      throw new ERR_INVALID_RETURN_VALUE(
        'an object', 'iterator.return()', result);
    }
  });
}

/**
 * Normalize the output of a fused run of stateless transforms for a batch.
 * @yields {Uint8Array[]}
 */
async function* yieldFusedStatelessOutput(current) {
  if (isUint8ArrayBatch(current)) {
    if (current.length > 0) yield current;
  } else if (isUint8Array(current)) {
    yield [current];
  } else if (typeof current === 'string') {
    yield [toUint8Array(current)];
  } else if (isAnyArrayBuffer(current)) {
    yield [new Uint8Array(current)];
  } else if (ArrayBufferIsView(current)) {
    yield [arrayBufferViewToUint8Array(current)];
  } else {
    yield* processTransformResultAsync(current);
  }
}

/**
 * Apply a fused run of stateless transforms to a batch from transform
 * `index` on, given the result of that transform, when the result has to be
 * waited for or normalized asynchronously.
 * @param {Array<Function>} run
 * @param {number} index
 * @param {any} result - The result of `run[index]`
 * @param {AbortSignal} signal
 * @yields {Uint8Array[]}
 */
async function* continueFusedStatelessBatch(run, index, result, signal) {
  let current;
  for (let i = index; i < run.length; i++) {
    if (i !== index) result = run[i](current, new TransformOptions(signal));
    if (isPromise(result)) result = await result;
    if (result === null) return;
    if (i === run.length - 1) {
      current = result;
      break;
    }
    current = normalizeTransformResultFast(result);
    if (current === undefined) {
      const normalized = [];
      const pendingResult = appendTransformResultAsync(normalized, result);
      if (pendingResult !== undefined) await pendingResult;
      current = normalized.length === 0 ? null : normalized[0];
    }
    if (current === null) return;
  }
  yield* yieldFusedStatelessOutput(current);
}

/**
 * Flush a fused run of stateless transforms once the source has ended:
 * flush each transform after all upstream data, including data emitted by
 * earlier flushes, has been processed by that transform.
 * @param {Array<Function>} run
 * @param {AbortSignal} signal
 * @yields {Uint8Array[]}
 */
async function* flushFusedStatelessAsyncTransforms(run, signal) {
  let pending = [];
  for (let i = 0; i < run.length; i++) {
    const next = [];
    for (let j = 0; j < pending.length; j++) {
      const pendingResult = appendTransformResultAsync(
        next,
        run[i](pending[j], new TransformOptions(signal)));
      if (pendingResult !== undefined) {
        await pendingResult;
      }
    }
    const flushResult = appendTransformResultAsync(
      next,
      run[i](null, new TransformOptions(signal)));
    if (flushResult !== undefined) {
      await flushResult;
    }
    pending = next;
  }
  for (let i = 0; i < pending.length; i++) {
    yield pending[i];
  }
}

// Returned by applyBatch() below when a batch is processed by a delegate.
const kDelegated = Symbol('kDelegated');

/**
 * Apply a fused run of stateless async transforms to a source.
 * All transforms in the run are applied in a tight synchronous loop per batch,
 * avoiding the overhead of N async generator ticks for N transforms.
 *
 * This is what an async generator looping over the source with for await
 * would do, written out by hand, as an async generator layer costs several
 * promises and an async frame for every batch: when the transforms return
 * batches or chunks synchronously, a batch takes a single promise. Results
 * that have to be waited for or normalized asynchronously, and the flush
 * once the source has ended, are handled by async generators. As with an
 * async generator looping over the source:
 * - the source is opened by the first next(), and calls made while one is in
 *   progress are queued;
 * - an error from the source ends the iteration without closing it, and an
 *   error from a transform closes it, ignoring errors from closing it;
 * - return() and throw() are passed to the async generator handling a batch,
 *   if any, and close the source unless it has ended; return() propagates
 *   errors from closing it, throw() ignores them.
 *
 * INVARIANT: This function accepts a signal, NOT a pre-built options object.
 * A fresh TransformOptions object is created for each
 * transform invocation to prevent cross-transform mutation.
 * @param {AsyncIterable<Uint8Array[]>} source
 * @param {Array<Function>} run - Array of stateless transform functions
 * @param {AbortSignal} signal - The pipeline's abort signal
 * @returns {AsyncIterator<Uint8Array[]>}
 */
function applyFusedStatelessAsyncTransforms(source, run, signal) {
  let state = kStart;
  let iterator;
  let nextMethod;
  // Whether the source has ended, and the run is being flushed.
  let sourceDone = false;
  // An async generator yielding the output of a batch, or of the flush.
  let delegate = null;
  const operations = createOperationQueue();

  function finish(result) {
    operations.settled();
    return result;
  }

  function fail(error) {
    state = kDone;
    operations.settled();
    throw error;
  }

  // Like for await when its body throws: close the source, keep the error.
  function onBodyError(error) {
    delegate = null;
    state = kDone;
    return PromisePrototypeThen(
      closeAsyncIterator(iterator, true), () => fail(error));
  }

  function onDelegateError(error) {
    if (!sourceDone) return onBodyError(error);
    delegate = null;
    return fail(error);
  }

  function onDelegateResult(result) {
    if (!result.done) return finish(new IterResult(false, result.value));
    delegate = null;
    if (!sourceDone) return pullSource();
    state = kDone;
    return finish(new IterResult(true, undefined));
  }

  function pullDelegate() {
    return PromisePrototypeThen(
      delegate.next(), onDelegateResult, onDelegateError);
  }

  // Apply the run to a batch synchronously: returns the output batch, null
  // if there is none, or kDelegated if `delegate` is to produce it.
  function applyBatch(chunks) {
    let current = chunks;
    for (let i = 0; i < run.length; i++) {
      const result = run[i](current, new TransformOptions(signal));
      if (isPromise(result)) {
        delegate = continueFusedStatelessBatch(run, i, result, signal);
        return kDelegated;
      }
      if (result === null) return null;
      if (i === run.length - 1) {
        current = result;
        break;
      }
      current = normalizeTransformResultFast(result);
      if (current === undefined) {
        delegate = continueFusedStatelessBatch(run, i, result, signal);
        return kDelegated;
      }
      if (current === null) return null;
    }
    if (isUint8ArrayBatch(current)) {
      return current.length > 0 ? current : null;
    }
    if (isUint8Array(current)) return [current];
    if (typeof current === 'string') return [toUint8Array(current)];
    if (isAnyArrayBuffer(current)) return [new Uint8Array(current)];
    if (ArrayBufferIsView(current)) {
      return [arrayBufferViewToUint8Array(current)];
    }
    delegate = processTransformResultAsync(current);
    return kDelegated;
  }

  function onSourceResult(result) {
    let value;
    try {
      if ((typeof result !== 'object' && typeof result !== 'function') ||
          result === null) {
        throw new ERR_INVALID_RETURN_VALUE(
          'an object', 'iterator.next()', result);
      }
      if (result.done) {
        sourceDone = true;
        delegate = flushFusedStatelessAsyncTransforms(run, signal);
        return pullDelegate();
      }
      value = result.value;
    } catch (error) {
      return fail(error);
    }
    let batch;
    try {
      batch = applyBatch(value);
    } catch (error) {
      return onBodyError(error);
    }
    if (batch === kDelegated) return pullDelegate();
    if (batch === null) return pullSource();
    return finish(new IterResult(false, batch));
  }

  function pullSource() {
    let promise;
    try {
      promise = PromiseResolve(FunctionPrototypeCall(nextMethod, iterator));
    } catch (error) {
      return fail(error);
    }
    return PromisePrototypeThen(promise, onSourceResult, fail);
  }

  function doNext() {
    if (state === kDone) {
      return PromiseResolve(finish(new IterResult(true, undefined)));
    }
    if (state === kStart) {
      try {
        iterator = source[SymbolAsyncIterator]();
        nextMethod = iterator.next;
      } catch (error) {
        state = kDone;
        operations.settled();
        return PromiseReject(error);
      }
      state = kActive;
    }
    try {
      return PromiseResolve(delegate !== null ? pullDelegate() : pullSource());
    } catch (error) {
      return PromiseReject(error);
    }
  }

  function doReturn(value) {
    const result = new IterResult(true, value);
    if (state !== kActive) {
      state = kDone;
      return PromiseResolve(finish(result));
    }
    state = kDone;
    const pending = delegate;
    delegate = null;
    const open = sourceDone ? null : iterator;
    let closed;
    if (pending === null) {
      closed = open === null ? kResolvedPromise :
        closeAsyncIterator(open, false);
    } else {
      // Close the delegate, then the source. If closing the delegate fails,
      // the source is still closed and the error kept.
      closed = PromisePrototypeThen(
        closeAsyncIterator(pending, false),
        () => (open === null ? undefined : closeAsyncIterator(open, false)),
        (error) => {
          if (open === null) throw error;
          return PromisePrototypeThen(closeAsyncIterator(open, true), () => {
            throw error;
          });
        });
    }
    return PromisePrototypeThen(closed, () => finish(result), fail);
  }

  function doThrow(error) {
    if (state !== kActive) {
      state = kDone;
      operations.settled();
      return PromiseReject(error);
    }
    state = kDone;
    const pending = delegate;
    delegate = null;
    // The delegate rethrows `error` (as yield* would see it do).
    let closed = pending === null ? kResolvedPromise :
      PromisePrototypeThen(pending.throw(error), undefined, () => {});
    if (!sourceDone) {
      closed = PromisePrototypeThen(
        closed, () => closeAsyncIterator(iterator, true));
    }
    return PromisePrototypeThen(closed, () => fail(error));
  }

  return ObjectSetPrototypeOf({
    next() { return operations.run(doNext); },
    return(value) { return operations.run(doReturn, value); },
    throw(error) { return operations.run(doThrow, error); },
    [SymbolAsyncIterator]() { return this; },
  }, null);
}

/**
 * Append a null flush signal after the source is exhausted.
 * @yields {Uint8Array[]}
 */
/**
 * Append a null flush signal after the source is exhausted.
 * @yields {Uint8Array[]}
 */
async function* withFlushAsync(source) {
  yield* source;
  yield null;
}

async function* applyStatefulAsyncTransform(
  source, transform, receiver, options) {
  const output = FunctionPrototypeCall(
    transform, receiver, withFlushAsync(source), options);
  for await (const item of output) {
    if (item === null) continue;
    // Fast path: item is already a Uint8Array[] batch (e.g. compression transforms)
    if (isUint8ArrayBatch(item)) {
      if (item.length > 0) {
        yield item;
      }
      continue;
    }
    // Fast path: single Uint8Array
    if (isUint8Array(item)) {
      yield [item];
      continue;
    }
    // Slow path: flatten arbitrary transform yield
    const batch = [];
    for await (const chunk of flattenTransformYieldAsync(item)) {
      ArrayPrototypePush(batch, chunk);
    }
    if (batch.length > 0) {
      yield batch;
    }
  }
}

/**
 * Fast path for validated stateful transforms (e.g. compression).
 * Skips withFlushAsync (transform handles done internally) and
 * skips isUint8ArrayBatch validation (transform guarantees valid output).
 * @yields {Uint8Array[]}
 */
async function* applyValidatedStatefulAsyncTransform(
  source, transform, receiver, options) {
  const output = FunctionPrototypeCall(
    transform, receiver, source, options);
  for await (const batch of output) {
    if (batch.length > 0) {
      yield batch;
    }
  }
  // Check abort after the transform completes - without the
  // withFlushAsync wrapper there is no extra yield to give
  // the outer pipeline a chance to see the abort.
  options.signal?.throwIfAborted();
}

/**
 * Create an async pipeline from source through transforms.
 * @param {AsyncIterable<Uint8Array[]>} source
 * @param {Array} transforms
 * @param {AbortSignal} [signal]
 * @returns {AsyncIterator<Uint8Array[]>}
 */
function createAsyncPipeline(source, transforms, signal) {
  if (transforms.length === 0) {
    return createAsyncPipelineWithoutTransforms(source, signal);
  }
  return createAsyncTransformPipeline(source, transforms, signal);
}

async function* createAsyncPipelineWithoutTransforms(source, signal) {
  // Check for abort
  signal?.throwIfAborted();
  yield* yieldAbortable(source, signal);
}

/**
 * Build the chain of transform layers of a pipeline.
 * @param {AsyncIterable<Uint8Array[]>} normalized
 * @param {Array} transforms
 * @param {AbortSignal} transformSignal
 * @returns {AsyncIterable<Uint8Array[]>}
 */
function createAsyncTransformLayers(normalized, transforms, transformSignal) {
  // Apply transforms - fuse consecutive stateless transforms into a single
  // layer to avoid unnecessary async ticks.
  //
  // INVARIANT: Each transform invocation MUST receive its own fresh options
  // object (new TransformOptions(signal)). Transforms may mutate the options
  // object, so sharing a single object across invocations would allow one
  // transform to corrupt the options seen by another. The signal is shared
  // across calls (mutations to it are acceptable), but the containing options
  // object must be unique per call. This is enforced inside
  // applyFusedStatelessAsyncTransforms and applyStatefulAsyncTransform, which
  // accept the signal directly and create the options object per invocation.
  // DO NOT pass a pre-built options object.
  let current = normalized;
  let statelessRun = [];

  for (let i = 0; i < transforms.length; i++) {
    const transform = transforms[i];
    if (isTransformObject(transform)) {
      // Flush any accumulated stateless run before the stateful transform
      if (statelessRun.length > 0) {
        current = applyFusedStatelessAsyncTransforms(current, statelessRun,
                                                     transformSignal);
        statelessRun = [];
      }
      const opts = new TransformOptions(transformSignal);
      if (transform[kValidatedTransform]) {
        current = applyValidatedStatefulAsyncTransform(
          current, transform.transform, transform.receiver, opts);
      } else {
        current = applyStatefulAsyncTransform(
          current, transform.transform, transform.receiver, opts);
      }
    } else {
      ArrayPrototypePush(statelessRun, transform);
    }
  }
  // Flush remaining stateless run
  if (statelessRun.length > 0) {
    current = applyFusedStatelessAsyncTransforms(current, statelessRun,
                                                 transformSignal);
  }
  return current;
}

/**
 * The pipeline through one or more transforms: an async iterator doing what
 * an async generator would, written out by hand to avoid an async generator
 * layer for every batch (see applyFusedStatelessAsyncTransforms()).
 *
 * When started by the first next(), it checks `signal`, then creates the
 * controller whose signal the transforms get, aborted when `signal` aborts.
 * Each batch, and the end, is passed on only if the transforms' signal has
 * not been aborted. If the pipeline fails, the transforms' signal is aborted
 * with the error; if it is stopped early by return() or throw(), the
 * transforms are closed and their signal is aborted.
 * @param {AsyncIterable<Uint8Array[]>} source
 * @param {Array} transforms
 * @param {AbortSignal} [signal]
 * @returns {AsyncIterator<Uint8Array[]>}
 */
function createAsyncTransformPipeline(source, transforms, signal) {
  let state = kStart;
  let controller;
  let abortHandler;
  let completed = false;
  let iterator;
  let nextMethod;
  const operations = createOperationQueue();

  // What an async generator would do in its `finally` block.
  function cleanup() {
    if (!completed && !controller.signal.aborted) {
      // Consumer stopped early or return() was called.
      // If a transform listener throws here, let it propagate.
      controller.abort(lazyDOMException('Aborted', 'AbortError'));
    }
    // Clean up user signal listener to prevent holding controller alive
    if (signal && abortHandler) {
      signal.removeEventListener('abort', abortHandler);
    }
  }

  function finish(result) {
    operations.settled();
    return result;
  }

  function complete(result) {
    state = kDone;
    try {
      cleanup();
    } finally {
      operations.settled();
    }
    return result;
  }

  // What an async generator would do in its `catch` block, then `finally`.
  function fail(error) {
    state = kDone;
    try {
      try {
        if (!controller.signal.aborted) {
          abortSignal(controller.signal, error);
        }
      } finally {
        cleanup();
      }
    } finally {
      operations.settled();
    }
    throw error;
  }

  function onResult(result) {
    let value;
    try {
      if ((typeof result !== 'object' && typeof result !== 'function') ||
          result === null) {
        throw new ERR_INVALID_RETURN_VALUE(
          'an object', 'iterator.next()', result);
      }
      if (result.done) {
        // A transform can abort while completing without producing a final
        // batch, for example when an async flush resolves to null. In that
        // case there is no batch with which to observe the abort.
        controller.signal.throwIfAborted();
        completed = true;
        return complete(new IterResult(true, undefined));
      }
      value = result.value;
    } catch (error) {
      return fail(error);
    }
    try {
      controller.signal.throwIfAborted();
    } catch (error) {
      // Like for await when its body throws: close the transforms first.
      return PromisePrototypeThen(
        closeAsyncIterator(iterator, true), () => fail(error));
    }
    return finish(new IterResult(false, value));
  }

  function pullTransforms() {
    let promise;
    try {
      promise = PromiseResolve(FunctionPrototypeCall(nextMethod, iterator));
    } catch (error) {
      return fail(error);
    }
    return PromisePrototypeThen(promise, onResult, fail);
  }

  function doNext() {
    if (state === kDone) {
      return PromiseResolve(finish(new IterResult(true, undefined)));
    }
    if (state === kStart) {
      try {
        // Check for abort
        signal?.throwIfAborted();
      } catch (error) {
        state = kDone;
        operations.settled();
        return PromiseReject(error);
      }
      state = kActive;
      const normalized = yieldAbortable(source, signal);
      // Create internal controller for transform cancellation.
      controller = new AbortController();
      if (signal) {
        abortHandler = () => {
          abortSignal(controller.signal, signal.reason);
        };
        signal.addEventListener('abort', abortHandler, kNullOnceOption);
      }
      try {
        const current = createAsyncTransformLayers(
          normalized, transforms, controller.signal);
        iterator = current[SymbolAsyncIterator]();
        nextMethod = iterator.next;
      } catch (error) {
        try {
          fail(error);
        } catch (failure) {
          return PromiseReject(failure);
        }
      }
    }
    try {
      return PromiseResolve(pullTransforms());
    } catch (error) {
      return PromiseReject(error);
    }
  }

  function doReturn(value) {
    const result = new IterResult(true, value);
    if (state !== kActive) {
      state = kDone;
      return PromiseResolve(finish(result));
    }
    state = kDone;
    return PromisePrototypeThen(
      closeAsyncIterator(iterator, false), () => complete(result), fail);
  }

  function doThrow(error) {
    if (state !== kActive) {
      state = kDone;
      operations.settled();
      return PromiseReject(error);
    }
    state = kDone;
    return PromisePrototypeThen(
      closeAsyncIterator(iterator, true), () => fail(error));
  }

  return ObjectSetPrototypeOf({
    next() { return operations.run(doNext); },
    return(value) { return operations.run(doReturn, value); },
    throw(error) { return operations.run(doThrow, error); },
    [SymbolAsyncIterator]() { return this; },
  }, null);
}

// =============================================================================
// Public API: pull() and pullSync()
// =============================================================================

/**
 * Create a sync pull-through pipeline with transforms.
 * @param {Iterable} source - The sync streamable source
 * @param {...Function} transforms - Variadic transforms
 * @returns {Iterable<Uint8Array[]>}
 */
function pullSync(source, ...transforms) {
  const normalized = fromSync(source);
  for (let i = 0; i < transforms.length; i++) {
    const transform = snapshotTransform(transforms[i]);
    if (transform === undefined) {
      throw new ERR_INVALID_ARG_TYPE(
        `transforms[${i}]`, ['Function', 'Object with transform()'],
        transforms[i]);
    }
    transforms[i] = transform;
  }
  return {
    __proto__: null,
    *[SymbolIterator]() {
      yield* createSyncPipeline(normalized, transforms);
    },
  };
}

/**
 * Create an async pull-through pipeline with transforms.
 * @param {Iterable|AsyncIterable} source - The streamable source
 * @param {...(Function|object)} args - Transforms, with optional PullOptions
 *   as last argument
 * @returns {AsyncIterable<Uint8Array[]>}
 */
function pull(source, ...args) {
  const parsed = parsePullArgs(args);
  const { transforms } = parsed;
  const options = converters.PullOptions(parsed.options, {
    __proto__: null,
    context: 'options',
  });
  const { signal } = options;
  const normalized = from(source);

  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      if (signal === undefined) {
        const controller = new AbortController();
        async function* pipeline() {
          yield* createAsyncPipeline(normalized, transforms, controller.signal);
        }
        const iterator = pipeline();
        return ObjectSetPrototypeOf({
          next(value) {
            return iterator.next(value);
          },
          return(value) {
            controller.abort(lazyDOMException('Aborted', 'AbortError'));
            return iterator.return(value);
          },
          throw(error) {
            abortSignal(controller.signal, error);
            return iterator.throw(error);
          },
          [SymbolAsyncIterator]() {
            return this;
          },
        }, null);
      }
      return createAbortablePullIterator(normalized, transforms, signal);
    },
  };
}

// Once `signal` aborts the pipeline, the pull that observed it rejects and
// so does every later pull, with the abort reason. That includes the case of
// an already-aborted signal, where the pipeline is never started. A plain
// async generator would instead complete after throwing, so later pulls
// would report a clean end of the stream.
function createAbortablePullIterator(source, transforms, signal) {
  let aborted = signal.aborted;
  let controller;
  let iterator;
  if (!aborted) {
    controller = new AbortController();
    const iteratorSignal = AbortSignal.any([signal, controller.signal]);
    async function* pipeline() {
      yield* createAsyncPipeline(source, transforms, iteratorSignal);
    }
    iterator = pipeline();
  }

  function onRejected(error) {
    if (signal.aborted) aborted = true;
    throw error;
  }

  return ObjectSetPrototypeOf({
    next(value) {
      if (aborted) return PromiseReject(signal.reason);
      return PromisePrototypeThen(iterator.next(value), undefined, onRejected);
    },
    return(value) {
      if (aborted) {
        return PromiseResolve(new IterResult(true, value));
      }
      controller.abort(lazyDOMException('Aborted', 'AbortError'));
      return iterator.return(value);
    },
    throw(error) {
      if (aborted) return PromiseReject(error);
      abortSignal(controller.signal, error);
      return PromisePrototypeThen(iterator.throw(error), undefined,
                                  onRejected);
    },
    [SymbolAsyncIterator]() {
      return this;
    },
  }, null);
}

// Keep ownership of a bonded consumer outside the transform pipeline so it can
// be detached even when the pipeline never starts or terminates early.
function pullWithConsumerCleanup(source, transforms, signal) {
  const sourceIterator = source[SymbolAsyncIterator]();
  const pipelineSource = {
    __proto__: null,
    [SymbolAsyncIterator]() {
      return sourceIterator;
    },
  };
  let sourceClosed = false;
  let abortHandler;

  function closeSource(method, value) {
    if (sourceClosed) return;
    sourceClosed = true;
    if (abortHandler !== undefined) {
      signal.removeEventListener('abort', abortHandler);
    }
    const close = sourceIterator[method] ?? sourceIterator.return;
    if (typeof close === 'function') {
      const result = FunctionPrototypeCall(close, sourceIterator, value);
      const cleanup = PromisePrototypeThen(
        PromiseResolve(result), undefined, undefined);
      markPromiseAsHandled(cleanup);
    }
  }

  if (signal?.aborted) {
    closeSource('throw', signal.reason);
    return {
      __proto__: null,
      // eslint-disable-next-line require-yield
      async *[SymbolAsyncIterator]() {
        throw signal.reason;
      },
    };
  }

  const pipeline = signal === undefined ?
    pull(pipelineSource, ...transforms) :
    pull(pipelineSource, ...transforms, { __proto__: null, signal });

  if (signal !== undefined) {
    abortHandler = () => closeSource('throw', signal.reason);
    signal.addEventListener('abort', abortHandler, kNullOnceOption);
    if (signal.aborted) abortHandler();
  }

  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      const iterator = pipeline[SymbolAsyncIterator]();
      return ObjectSetPrototypeOf({
        next(value) {
          return PromisePrototypeThen(
            iterator.next(value),
            (result) => {
              if (result.done) closeSource('return');
              return result;
            },
            (error) => {
              closeSource('throw', error);
              throw error;
            });
        },
        return(value) {
          closeSource('return', value);
          return iterator.return(value);
        },
        throw(error) {
          closeSource('throw', error);
          return iterator.throw(error);
        },
        [SymbolAsyncIterator]() {
          return this;
        },
      }, null);
    },
  };
}

// =============================================================================
// Public API: pipeTo() and pipeToSync()
// =============================================================================

/**
 * Write a sync source through transforms to a sync writer.
 * @param {Iterable<Uint8Array[]>} source
 * @param {...(Function|object)} args - Transforms, writer, and optional options
 * @returns {number} Total bytes written
 */
function pipeToSync(source, ...args) {
  const parsed = parsePipeToArgs(args, 'writeSync');
  const { transforms, writer } = parsed;
  const options = converters.PipeToSyncOptions(parsed.options, {
    __proto__: null,
    context: 'options',
  });
  const hasWritevSync = typeof writer.writevSync === 'function';
  // endSync() is optional: a writer without it is not closed.
  const endSync = writer.endSync;
  const hasEndSync = typeof endSync === 'function';

  // Normalize source and create pipeline
  const normalized = fromSync(source);
  const pipeline = transforms.length > 0 ?
    createSyncPipeline(normalized, transforms) :
    normalized;

  let totalBytes = 0;
  let closedSync = true;

  try {
    for (const batch of pipeline) {
      // Single chunk, the common case: no batch entry needed.
      if (batch.length === 1) {
        const chunk = batch[0];
        if (callWithByteView(chunk, writer.writeSync, writer) === false) {
          throw new ERR_OUT_OF_RANGE(
            'write', 'within byte budget', 'budget exhausted');
        }
        totalBytes += TypedArrayPrototypeGetByteLength(chunk);
        continue;
      }
      const entry = createBatchEntry(batch);
      if (hasWritevSync && batch.length > 1) {
        const accepted = writer.writevSync(validateBatchEntry(entry));
        validateBatchEntry(entry);
        if (accepted === false) {
          throw new ERR_OUT_OF_RANGE(
            'write', 'within byte budget', 'budget exhausted');
        }
        totalBytes += entry.byteLength;
      } else {
        for (let i = 0; i < entry.views.length; i++) {
          const view = entry.views[i];
          const chunk = validateByteView(view);
          const accepted = writer.writeSync(chunk);
          validateByteView(view);
          if (accepted === false) {
            throw new ERR_OUT_OF_RANGE(
              'write', 'within byte budget', 'budget exhausted');
          }
          totalBytes += view.byteLength;
        }
      }
    }

    if (!options.preventClose && hasEndSync) {
      closedSync = FunctionPrototypeCall(endSync, writer) >= 0;
    }
  } catch (error) {
    if (!options.preventFail) {
      failWriterQuietly(writer, error);
    }
    throw error;
  }

  // endSync() returning -1 only means that the writer cannot close
  // synchronously; every chunk was accepted. pipeToSync() never falls back to
  // the async end(), so report it. By default the writer is left as it is so
  // that the caller can still close it (e.g. with `await writer.end()`);
  // `failOnIncompleteClose` fails it instead, for callers that cannot.
  if (!closedSync) {
    const error = new ERR_INVALID_STATE(
      'Writer could not be closed synchronously');
    if (options.failOnIncompleteClose && !options.preventFail) {
      failWriterQuietly(writer, error);
    }
    throw error;
  }

  return totalBytes;
}

// Call writer.fail(error) on a best-effort basis. The error that made the
// pipe fail is what the caller must see; an exception from fail() itself
// must not replace it.
function failWriterQuietly(writer, error) {
  try {
    writer.fail?.(error);
  } catch {
    // Ignored, see above.
  }
}

/**
 * Write an async source through transforms to a writer.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {...(Function|object)} args - Transforms, writer, and optional options
 * @returns {Promise<number>} Total bytes written
 */
async function pipeTo(source, ...args) {
  const parsed = parsePipeToArgs(args, 'write');
  const { transforms, writer } = parsed;
  const options = converters.PipeToOptions(parsed.options, {
    __proto__: null,
    context: 'options',
  });
  const { signal } = options;

  function failWriter(error) {
    if (!options.preventFail) {
      failWriterQuietly(writer, error);
    }
  }

  try {
    signal?.throwIfAborted();
  } catch (error) {
    failWriter(error);
    throw error;
  }

  const hasWriteSync = typeof writer.writeSync === 'function';
  const normalized = from(source);

  let totalBytes = 0;
  const hasWritev = typeof writer.writev === 'function';
  const hasWritevSync = typeof writer.writevSync === 'function';
  const hasEndSync = typeof writer.endSync === 'function';

  // Async fallback for writeBatch when sync write fails partway through.
  // Continues writing from batch[startIndex] using async write().
  async function writeBatchAsyncFallback(entry, startIndex) {
    for (let i = startIndex; i < entry.views.length; i++) {
      const view = entry.views[i];
      if (hasWriteSync) {
        const chunk = validateByteView(view);
        if (writer.writeSync(chunk)) {
          validateByteView(view);
          totalBytes += view.byteLength;
          continue;
        }
        validateByteView(view);
      }
      const result = writer.write(
        validateByteView(view),
        signal ? { __proto__: null, signal } : undefined);
      if (result !== undefined) {
        await result;
      }
      validateByteView(view);
      totalBytes += view.byteLength;
    }
  }

  // Write a batch using try-fallback: sync first, async if needed.
  // Returns undefined on sync success, or a Promise when async fallback
  // is required. Callers must check: const p = writeBatch(b); if (p) await p;
  function writeBatch(batch) {
    // Single chunk, the common case: check the view around writeSync()
    // without allocating a batch entry, and create one only to fall back to
    // the async path.
    if (batch.length === 1 && hasWriteSync) {
      const chunk = batch[0];
      if (callWithByteView(chunk, writer.writeSync, writer)) {
        totalBytes += TypedArrayPrototypeGetByteLength(chunk);
        return;
      }
      return writeBatchAsyncFallback(createBatchEntry(batch), 0);
    }
    const entry = createBatchEntry(batch);
    if (hasWritev && batch.length > 1) {
      if (!hasWritevSync ||
          !writer.writevSync(validateBatchEntry(entry))) {
        validateBatchEntry(entry);
        const opts = signal ? { __proto__: null, signal } : undefined;
        const writevResult = writer.writev(validateBatchEntry(entry), opts);
        if (writevResult === undefined) {
          validateBatchEntry(entry);
          totalBytes += entry.byteLength;
          return;
        }
        return PromisePrototypeThen(PromiseResolve(writevResult), () => {
          validateBatchEntry(entry);
          totalBytes += entry.byteLength;
        });
      }
      validateBatchEntry(entry);
      totalBytes += entry.byteLength;
      return;
    }
    for (let i = 0; i < entry.views.length; i++) {
      const view = entry.views[i];
      const chunk = validateByteView(view);
      if (!hasWriteSync || !writer.writeSync(chunk)) {
        if (hasWriteSync) validateByteView(view);
        // Sync path failed at index i - fall back to async for the rest.
        return writeBatchAsyncFallback(entry, i);
      }
      validateByteView(view);
      totalBytes += view.byteLength;
    }
  }

  try {
    if (transforms.length === 0) {
      // Fast path: no transforms - iterate normalized source directly
      if (signal) {
        for await (const batch of yieldAbortable(normalized, signal)) {
          signal.throwIfAborted();
          const p = writeBatch(batch);
          if (p) await p;
        }
      } else {
        for await (const batch of normalized) {
          const p = writeBatch(batch);
          if (p) await p;
        }
      }
    } else {
      const pipeline = createAsyncPipeline(normalized, transforms, signal);

      if (signal) {
        for await (const batch of pipeline) {
          signal.throwIfAborted();
          const p = writeBatch(batch);
          if (p) await p;
        }
      } else {
        for await (const batch of pipeline) {
          const p = writeBatch(batch);
          if (p) await p;
        }
      }
    }

    if (!options.preventClose) {
      if (!hasEndSync || writer.endSync() < 0) {
        await writer.end?.(signal ? { __proto__: null, signal } : undefined);
      }
    }
  } catch (error) {
    failWriter(error);
    throw error;
  }

  return totalBytes;
}

module.exports = {
  pipeTo,
  pipeToSync,
  pull,
  pullSync,
  pullWithConsumerCleanup,
};
