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
  PromisePrototypeThen,
  SymbolAsyncIterator,
  SymbolIterator,
  TypedArrayPrototypeGetByteLength,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');
const { lazyDOMException } = require('internal/util');
const { validateAbortSignal } = require('internal/validators');
const {
  isAnyArrayBuffer,
  isPromise,
  isUint8Array,
} = require('internal/util/types');
const { AbortController } = require('internal/abort_controller');

const {
  from,
  fromSync,
  primitiveToUint8Array,
  isSyncIterable,
  isAsyncIterable,
  isUint8ArrayBatch,
} = require('internal/streams/iter/from');

const {
  isPullOptions,
  isTransform,
  isTransformObject,
  parsePullArgs,
  toUint8Array,
  wrapError,
} = require('internal/streams/iter/utils');

const {
  kTrustedTransform,
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
 * @returns {{ transforms: Array, writer: object, options: object }}
 */
function parsePipeToArgs(args, requiredMethod) {
  if (args.length === 0) {
    throw new ERR_INVALID_ARG_VALUE('args', args, 'pipeTo requires a writer argument');
  }

  let options;
  let writerIndex = args.length - 1;

  // Check if last arg is options
  const last = args[args.length - 1];
  if (isPullOptions(last) && !hasMethod(last, requiredMethod)) {
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
    yield primitiveToUint8Array(value);
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
    yield primitiveToUint8Array(value);
    return;
  }
  // Check for async iterable first
  if (isAsyncIterable(value)) {
    for await (const item of value) {
      yield* flattenTransformYieldAsync(item);
    }
    return;
  }
  // Must be sync Iterable<TransformYield>
  if (isSyncIterable(value)) {
    for (const item of value) {
      yield* flattenTransformYieldAsync(item);
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
    yield [primitiveToUint8Array(result)];
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
    yield [primitiveToUint8Array(result)];
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
      for await (const chunk of flattenTransformYieldAsync(item)) {
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
      current = result;
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
      yield [primitiveToUint8Array(current)];
    } else {
      yield* processTransformResultSync(current);
    }
  }
  // Flush
  let current = null;
  for (let i = 0; i < run.length; i++) {
    const result = run[i](current);
    if (result === null) {
      current = null;
      continue;
    }
    current = result;
  }
  if (current != null) {
    yield* processTransformResultSync(current);
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

function* applyStatefulSyncTransform(source, transform) {
  const output = transform(withFlushSync(source));
  for (const item of output) {
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
      current = applyStatefulSyncTransform(current, transform.transform);
    } else {
      statelessRun.push(transform);
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
      const result = run[i](current, { __proto__: null, signal });
      if (result === null) {
        current = null;
        break;
      }
      if (isPromise(result)) {
        const resolved = await result;
        if (resolved === null) {
          current = null;
          break;
        }
        current = resolved;
      } else {
        current = result;
      }
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
      yield [primitiveToUint8Array(current)];
    } else {
      yield* processTransformResultAsync(current);
    }
  }
  // Flush: send null through each transform in order
  let current = null;
  for (let i = 0; i < run.length; i++) {
    const result = run[i](current, { __proto__: null, signal });
    if (result === null) {
      current = null;
      continue;
    }
    if (isPromise(result)) {
      current = await result;
    } else {
      current = result;
    }
  }
  if (current !== null) {
    if (isUint8ArrayBatch(current)) {
      if (current.length > 0) yield current;
    } else if (isUint8Array(current)) {
      yield [current];
    } else if (typeof current === 'string') {
      yield [toUint8Array(current)];
    } else {
      yield* processTransformResultAsync(current);
    }
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
  for await (const batch of source) {
    yield batch;
  }
  yield null;
}

async function* applyStatefulAsyncTransform(source, transform, options) {
  const output = transform(withFlushAsync(source), options);
  for await (const item of output) {
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
 * Fast path for trusted stateful transforms (e.g. compression).
 * Skips withFlushAsync (transform handles done internally) and
 * skips isUint8ArrayBatch validation (transform guarantees valid output).
 * @yields {Uint8Array[]}
 */
async function* applyTrustedStatefulAsyncTransform(source, transform, options) {
  const output = transform(source, options);
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

  const normalized = source;

  // Fast path: no transforms, just yield normalized source directly
  if (transforms.length === 0) {
    for await (const batch of normalized) {
      signal?.throwIfAborted();
      yield batch;
    }
    return;
  }

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
      if (transform[kTrustedTransform]) {
        current = applyTrustedStatefulAsyncTransform(
          current, transform.transform, opts);
      } else {
        current = applyStatefulAsyncTransform(
          current, transform.transform, opts);
      }
    } else {
      statelessRun.push(transform);
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
      yield* createSyncPipeline(fromSync(source), transforms);
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
  const { transforms, options } = parsePullArgs(args);
  const signal = options?.signal;
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
    // Eagerly check abort at call time per spec
    if (signal.aborted) {
      return {
        __proto__: null,
        // eslint-disable-next-line require-yield
        async *[SymbolAsyncIterator]() {
          throw signal.reason;
        },
      };
    }
  }

  return {
    __proto__: null,
    async *[SymbolAsyncIterator]() {
      yield* createAsyncPipeline(from(source), transforms, signal);
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
  const { transforms, writer, options } = parsePipeToArgs(args, 'writeSync');

  // Handle transform-writer
  if (isTransformObject(writer)) {
    ArrayPrototypePush(transforms, writer);
  }

  // Normalize source and create pipeline
  const normalized = fromSync(source);
  const pipeline = transforms.length > 0 ?
    createSyncPipeline(normalized, transforms) :
    normalized;

  let totalBytes = 0;
  const hasWriteSync = typeof writer.writeSync === 'function';
  const hasWritevSync = typeof writer.writevSync === 'function';
  const hasEndSync = typeof writer.endSync === 'function';

  try {
    for (const batch of pipeline) {
      if (hasWritevSync && batch.length > 1) {
        writer.writevSync(batch);
        for (let i = 0; i < batch.length; i++) {
          totalBytes += TypedArrayPrototypeGetByteLength(batch[i]);
        }
      } else {
        for (let i = 0; i < batch.length; i++) {
          const chunk = batch[i];
          if (hasWriteSync) {
            writer.writeSync(chunk);
          } else {
            writer.write(chunk);
          }
          totalBytes += TypedArrayPrototypeGetByteLength(chunk);
        }
      }
    }

    if (!options?.preventClose) {
      if (!hasEndSync || writer.endSync() < 0) {
        writer.end?.();
      }
    }
  } catch (error) {
    if (!options?.preventFail) {
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
  const { transforms, writer, options } = parsePipeToArgs(args, 'write');
  if (options?.signal !== undefined) {
    validateAbortSignal(options.signal, 'options.signal');
  }

  // Handle transform-writer
  if (isTransformObject(writer)) {
    ArrayPrototypePush(transforms, writer);
  }

  const signal = options?.signal;

  // Check for abort
  signal?.throwIfAborted();

  // Normalize source via from()
  const normalized = from(source);

  let totalBytes = 0;
  const hasWritev = typeof writer.writev === 'function';
  const hasWriteSync = typeof writer.writeSync === 'function';
  const hasWritevSync = typeof writer.writevSync === 'function';
  const hasEndSync = typeof writer.endSync === 'function';
  // Async fallback for writeBatch when sync write fails partway through.
  // Continues writing from batch[startIndex] using async write().
  async function writeBatchAsyncFallback(batch, startIndex) {
    for (let i = startIndex; i < batch.length; i++) {
      const chunk = batch[i];
      if (hasWriteSync && writer.writeSync(chunk)) {
        // Sync retry succeeded
      } else {
        const result = writer.write(
          chunk, signal ? { __proto__: null, signal } : undefined);
        if (result !== undefined) {
          await result;
        }
      }
      totalBytes += TypedArrayPrototypeGetByteLength(chunk);
    }
  }

  // Write a batch using try-fallback: sync first, async if needed.
  // Returns undefined on sync success, or a Promise when async fallback
  // is required. Callers must check: const p = writeBatch(b); if (p) await p;
  function writeBatch(batch) {
    if (hasWritev && batch.length > 1) {
      if (!hasWritevSync || !writer.writevSync(batch)) {
        const opts = signal ? { __proto__: null, signal } : undefined;
        return PromisePrototypeThen(writer.writev(batch, opts), () => {
          for (let i = 0; i < batch.length; i++) {
            totalBytes += TypedArrayPrototypeGetByteLength(batch[i]);
          }
        });
      }
      for (let i = 0; i < batch.length; i++) {
        totalBytes += TypedArrayPrototypeGetByteLength(batch[i]);
      }
      return;
    }
    for (let i = 0; i < batch.length; i++) {
      const chunk = batch[i];
      if (!hasWriteSync || !writer.writeSync(chunk)) {
        // Sync path failed at index i - fall back to async for the rest.
        // Count bytes for chunks already written synchronously (0..i-1).
        return writeBatchAsyncFallback(batch, i);
      }
      totalBytes += TypedArrayPrototypeGetByteLength(chunk);
    }
  }

  try {
    // Fast path: no transforms - iterate normalized source directly
    if (transforms.length === 0) {
      if (signal) {
        for await (const batch of normalized) {
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

    if (!options?.preventClose) {
      if (!hasEndSync || writer.endSync() < 0) {
        await writer.end?.(signal ? { __proto__: null, signal } : undefined);
      }
    }
  } catch (error) {
    if (!options?.preventFail) {
      writer.fail?.(wrapError(error));
    }
    throw error;
  }

  return totalBytes;
}

module.exports = {
  pipeTo,
  pipeToSync,
  pull,
  pullSync,
};
