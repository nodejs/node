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
  PromisePrototypeThen,
  PromiseResolve,
  SymbolAsyncIterator,
  SymbolIterator,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
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
  createBatchEntry,
  isTransform,
  isTransformObject,
  parsePullArgs,
  toUint8Array,
  validateBatchEntry,
  validateByteView,
  wrapError,
  yieldAbortable,
} = require('internal/streams/iter/utils');
const {
  converters,
} = require('internal/streams/iter/webidl');

const {
  kValidatedTransform,
} = require('internal/streams/iter/types');

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
  if (!isTransform(last) && !hasMethod(last, requiredMethod)) {
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
    if (!isTransform(transforms[i])) {
      throw new ERR_INVALID_ARG_TYPE(
        `transforms[${i}]`, ['Function', 'Object with transform()'],
        transforms[i]);
    }
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
        current, transform.transform, transform);
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

/**
 * Apply a single stateless async transform to a source.
 * @yields {Uint8Array[]}
 */
/**
 * Apply a fused run of stateless async transforms to a source.
 * All transforms in the run are applied in a tight synchronous loop per batch,
 * avoiding the overhead of N async generator ticks for N transforms.
 *
 * INVARIANT: This function accepts a signal, NOT a pre-built options object.
 * A fresh { __proto__: null, signal } options object is created for each
 * transform invocation to prevent cross-transform mutation.
 * @param {AsyncIterable<Uint8Array[]>} source
 * @param {Array<Function>} run - Array of stateless transform functions
 * @param {AbortSignal} signal - The pipeline's abort signal
 * @yields {Uint8Array[]}
 */
async function* applyFusedStatelessAsyncTransforms(source, run, signal) {
  for await (const chunks of source) {
    let current = chunks;
    for (let i = 0; i < run.length; i++) {
      let result = run[i](current, { __proto__: null, signal });
      if (isPromise(result)) result = await result;
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
        const pendingResult = appendTransformResultAsync(normalized, result);
        if (pendingResult !== undefined) await pendingResult;
        current = normalized.length === 0 ? null : normalized[0];
      }
      if (current === null) break;
    }
    if (current === null) continue;
    // Normalize the final output
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
  // Flush each transform after all upstream data, including data emitted by
  // earlier flushes, has been processed by that transform.
  let pending = [];
  for (let i = 0; i < run.length; i++) {
    const next = [];
    for (let j = 0; j < pending.length; j++) {
      const pendingResult = appendTransformResultAsync(
        next,
        run[i](pending[j], { __proto__: null, signal }));
      if (pendingResult !== undefined) {
        await pendingResult;
      }
    }
    const flushResult = appendTransformResultAsync(
      next,
      run[i](null, { __proto__: null, signal }));
    if (flushResult !== undefined) {
      await flushResult;
    }
    pending = next;
  }
  for (let i = 0; i < pending.length; i++) {
    yield pending[i];
  }
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
 * @yields {Uint8Array[]}
 */
async function* createAsyncPipeline(source, transforms, signal) {
  // Check for abort
  signal?.throwIfAborted();

  // Fast path: no transforms, just yield normalized source directly
  if (transforms.length === 0) {
    yield* yieldAbortable(source, signal);
    return;
  }

  const normalized = yieldAbortable(source, signal);

  // Create internal controller for transform cancellation.
  // Note: if signal was already aborted, we threw above - no need to check here.
  const controller = new AbortController();
  let abortHandler;
  if (signal) {
    abortHandler = () => {
      controller.abort(signal.reason ??
        lazyDOMException('Aborted', 'AbortError'));
    };
    signal.addEventListener('abort', abortHandler, { __proto__: null, once: true });
  }

  // Apply transforms - fuse consecutive stateless transforms into a single
  // generator layer to avoid unnecessary async generator ticks.
  //
  // INVARIANT: Each transform invocation MUST receive its own fresh options
  // object ({ __proto__: null, signal }). Transforms may mutate the options
  // object, so sharing a single object across invocations would allow one
  // transform to corrupt the options seen by another. The signal is shared
  // across calls (mutations to it are acceptable), but the containing options
  // object must be unique per call. This is enforced inside
  // applyFusedStatelessAsyncTransforms and applyStatefulAsyncTransform, which
  // accept the signal directly and create the options object per invocation.
  // DO NOT pass a pre-built options object.
  let current = normalized;
  const transformSignal = controller.signal;
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
      const opts = { __proto__: null, signal: transformSignal };
      if (transform[kValidatedTransform]) {
        current = applyValidatedStatefulAsyncTransform(
          current, transform.transform, transform, opts);
      } else {
        current = applyStatefulAsyncTransform(
          current, transform.transform, transform, opts);
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

  let completed = false;
  try {
    for await (const batch of current) {
      controller.signal.throwIfAborted();
      yield batch;
    }
    // A transform can abort while completing without producing a final batch,
    // for example when an async flush resolves to null. In that case the loop
    // body has no opportunity to observe the abort.
    controller.signal.throwIfAborted();
    completed = true;
  } catch (error) {
    if (!controller.signal.aborted) {
      controller.abort(wrapError(error));
    }
    throw error;
  } finally {
    if (!completed && !controller.signal.aborted) {
      // Consumer stopped early or generator return() was called.
      // If a transform listener throws here, let it propagate.
      controller.abort(lazyDOMException('Aborted', 'AbortError'));
    }
    // Clean up user signal listener to prevent holding controller alive
    if (signal && abortHandler) {
      signal.removeEventListener('abort', abortHandler);
    }
  }
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
    if (!isTransform(transforms[i])) {
      throw new ERR_INVALID_ARG_TYPE(
        `transforms[${i}]`, ['Function', 'Object with transform()'],
        transforms[i]);
    }
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
  signal?.throwIfAborted();

  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      const controller = new AbortController();
      const iteratorSignal = signal === undefined ?
        controller.signal : AbortSignal.any([signal, controller.signal]);

      async function* pipeline() {
        yield* createAsyncPipeline(normalized, transforms, iteratorSignal);
      }
      const iterator = pipeline();

      return {
        __proto__: null,
        next(value) {
          return iterator.next(value);
        },
        return(value) {
          controller.abort(lazyDOMException('Aborted', 'AbortError'));
          return iterator.return(value);
        },
        throw(error) {
          controller.abort(error);
          return iterator.throw(error);
        },
        [SymbolAsyncIterator]() {
          return this;
        },
      };
    },
  };
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
      PromisePrototypeThen(PromiseResolve(result), undefined, () => {});
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
    signal.addEventListener('abort', abortHandler,
                            { __proto__: null, once: true });
    if (signal.aborted) abortHandler();
  }

  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      const iterator = pipeline[SymbolAsyncIterator]();
      return {
        __proto__: null,
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
      };
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
  const endSync = writer.endSync;

  if (!options.preventClose && typeof endSync !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'writer.endSync', 'Function', endSync);
  }

  // Normalize source and create pipeline
  const normalized = fromSync(source);
  const pipeline = transforms.length > 0 ?
    createSyncPipeline(normalized, transforms) :
    normalized;

  let totalBytes = 0;

  try {
    for (const batch of pipeline) {
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

    if (!options.preventClose) {
      if (FunctionPrototypeCall(endSync, writer) < 0) {
        throw new ERR_INVALID_STATE(
          'Writer could not be closed synchronously');
      }
    }
  } catch (error) {
    if (!options.preventFail) {
      writer.fail?.(wrapError(error));
    }
    throw error;
  }

  return totalBytes;
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
      writer.fail?.(wrapError(error));
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
