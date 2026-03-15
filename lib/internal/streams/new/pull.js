'use strict';

// New Streams API - Pull Pipeline
//
// pull(), pullSync(), pipeTo(), pipeToSync()
// Pull-through pipelines with transforms. Data flows on-demand from source
// through transforms to consumer.

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  Error,
  Promise,
  SafePromiseAllReturnVoid,
  String,
  SymbolAsyncIterator,
  SymbolIterator,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OPERATION_FAILED,
  },
} = require('internal/errors');
const { TextEncoder } = require('internal/encoding');
const { lazyDOMException } = require('internal/util');
const { AbortController } = require('internal/abort_controller');

const {
  normalizeAsyncSource,
  normalizeSyncSource,
  isSyncIterable,
  isAsyncIterable,
  isUint8ArrayBatch,
} = require('internal/streams/new/from');

const {
  isPullOptions,
  parsePullArgs,
} = require('internal/streams/new/utils');

// Shared TextEncoder instance for string conversion.
const encoder = new TextEncoder();

// =============================================================================
// Type Guards and Helpers
// =============================================================================

/**
 * Check if a value is a TransformObject (has transform property).
 * @returns {boolean}
 */
function isTransformObject(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    'transform' in value &&
    typeof value.transform === 'function'
  );
}

/**
 * Check if a value is a Writer (has write method).
 * @returns {boolean}
 */
function isWriter(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    'write' in value &&
    typeof value.write === 'function'
  );
}

/**
 * Parse variadic arguments for pipeTo/pipeToSync.
 * Returns { transforms, writer, options }
 * @returns {object}
 */
function parsePipeToArgs(args) {
  if (args.length === 0) {
    throw new ERR_INVALID_ARG_VALUE('args', args, 'pipeTo requires a writer argument');
  }

  let options;
  let writerIndex = args.length - 1;

  // Check if last arg is options
  const last = args[args.length - 1];
  if (isPullOptions(last) && !isWriter(last)) {
    options = last;
    writerIndex = args.length - 2;
  }

  if (writerIndex < 0) {
    throw new ERR_INVALID_ARG_VALUE('args', args, 'pipeTo requires a writer argument');
  }

  const writer = args[writerIndex];
  if (!isWriter(writer)) {
    throw new ERR_INVALID_ARG_TYPE('writer', 'object with a write method', writer);
  }

  return {
    transforms: ArrayPrototypeSlice(args, 0, writerIndex),
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
  if (value instanceof Uint8Array) {
    yield value;
    return;
  }
  if (typeof value === 'string') {
    yield encoder.encode(value);
    return;
  }
  // Must be Iterable<TransformYield>
  if (isSyncIterable(value)) {
    for (const item of value) {
      yield* flattenTransformYieldSync(item);
    }
    return;
  }
  throw new ERR_INVALID_ARG_TYPE('value', ['Uint8Array', 'string', 'Iterable'], value);
}

/**
 * Flatten transform yield to Uint8Array chunks (async).
 * @yields {Uint8Array}
 */
async function* flattenTransformYieldAsync(value) {
  if (value instanceof Uint8Array) {
    yield value;
    return;
  }
  if (typeof value === 'string') {
    yield encoder.encode(value);
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
  throw new ERR_INVALID_ARG_TYPE('value', ['Uint8Array', 'string', 'Iterable', 'AsyncIterable'], value);
}

/**
 * Process transform result (sync).
 * @yields {Uint8Array[]}
 */
function* processTransformResultSync(result) {
  if (result === null) {
    return;
  }
  if (ArrayIsArray(result) && result.length > 0 &&
      result[0] instanceof Uint8Array) {
    // Fast path: Uint8Array[]
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
  throw new ERR_INVALID_ARG_TYPE('result', ['Array', 'Iterable'], result);
}

/**
 * Process transform result (async).
 * @yields {Uint8Array[]}
 */
async function* processTransformResultAsync(result) {
  // Handle Promise
  if (result instanceof Promise) {
    const resolved = await result;
    yield* processTransformResultAsync(resolved);
    return;
  }
  if (result === null) {
    return;
  }
  if (ArrayIsArray(result) &&
      (result.length === 0 || result[0] instanceof Uint8Array)) {
    // Fast path: Uint8Array[]
    if (result.length > 0) {
      yield result;
    }
    return;
  }
  // Check for async iterable/generator first
  if (isAsyncIterable(result)) {
    const batch = [];
    for await (const item of result) {
      // Fast path: item is already Uint8Array
      if (item instanceof Uint8Array) {
        ArrayPrototypePush(batch, item);
        continue;
      }
      // Slow path: flatten the item
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
      // Fast path: item is already Uint8Array
      if (item instanceof Uint8Array) {
        ArrayPrototypePush(batch, item);
        continue;
      }
      // Slow path: flatten the item
      for await (const chunk of flattenTransformYieldAsync(item)) {
        ArrayPrototypePush(batch, chunk);
      }
    }
    if (batch.length > 0) {
      yield batch;
    }
    return;
  }
  throw new ERR_INVALID_ARG_TYPE('result', ['Array', 'Iterable', 'AsyncIterable'], result);
}

// =============================================================================
// Sync Pipeline Implementation
// =============================================================================

/**
 * Apply a single stateless sync transform to a source.
 * @yields {Uint8Array[]}
 */
function* applyStatelessSyncTransform(source, transform) {
  for (const chunks of source) {
    const result = transform(chunks);
    yield* processTransformResultSync(result);
  }
}

/**
 * Apply a single stateful sync transform to a source.
 * @yields {Uint8Array[]}
 */
function* applyStatefulSyncTransform(source, transform) {
  const output = transform(source);
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
 * Wrap sync source to add null flush signal at end.
 * @yields {Uint8Array[]}
 */
function* withFlushSignalSync(source) {
  for (const batch of source) {
    yield batch;
  }
  yield null; // Flush signal
}

/**
 * Create a sync pipeline from source through transforms.
 * @yields {Uint8Array[]}
 */
function* createSyncPipeline(source, transforms) {
  // Normalize source
  let current = withFlushSignalSync(normalizeSyncSource(source));

  // Apply transforms - Object = stateful, function = stateless
  for (let i = 0; i < transforms.length; i++) {
    const transform = transforms[i];
    if (isTransformObject(transform)) {
      current = applyStatefulSyncTransform(current, transform.transform);
    } else {
      current = applyStatelessSyncTransform(current, transform);
    }
  }

  // Yield results (filter out null from final output)
  for (const batch of current) {
    if (batch !== null) {
      yield batch;
    }
  }
}

// =============================================================================
// Async Pipeline Implementation
// =============================================================================

/**
 * Apply a single stateless async transform to a source.
 * @yields {Uint8Array[]}
 */
async function* applyStatelessAsyncTransform(source, transform, options) {
  for await (const chunks of source) {
    const result = transform(chunks, options);
    // Fast path: result is already Uint8Array[] (common case)
    if (result === null) continue;
    if (isUint8ArrayBatch(result)) {
      if (result.length > 0) {
        yield result;
      }
      continue;
    }
    // Handle Promise of Uint8Array[]
    if (result instanceof Promise) {
      const resolved = await result;
      if (resolved === null) continue;
      if (isUint8ArrayBatch(resolved)) {
        if (resolved.length > 0) {
          yield resolved;
        }
        continue;
      }
      // Fall through to slow path
      yield* processTransformResultAsync(resolved);
      continue;
    }
    // Fast path: sync generator/iterable - collect all yielded items
    if (isSyncIterable(result) && !isAsyncIterable(result)) {
      const batch = [];
      for (const item of result) {
        if (isUint8ArrayBatch(item)) {
          for (let i = 0; i < item.length; i++) {
            ArrayPrototypePush(batch, item[i]);
          }
        } else if (item instanceof Uint8Array) {
          ArrayPrototypePush(batch, item);
        } else if (item !== null && item !== undefined) {
          for await (const chunk of flattenTransformYieldAsync(item)) {
            ArrayPrototypePush(batch, chunk);
          }
        }
      }
      if (batch.length > 0) {
        yield batch;
      }
      continue;
    }
    // Slow path for other types
    yield* processTransformResultAsync(result);
  }
}

/**
 * Apply a single stateful async transform to a source.
 * @yields {Uint8Array[]}
 */
async function* applyStatefulAsyncTransform(source, transform, options) {
  const output = transform(source, options);
  for await (const item of output) {
    // Fast path: item is already a Uint8Array[] batch (e.g. compression transforms)
    if (isUint8ArrayBatch(item)) {
      if (item.length > 0) {
        yield item;
      }
      continue;
    }
    // Fast path: single Uint8Array
    if (item instanceof Uint8Array) {
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
 * Wrap async source to add null flush signal at end.
 * @yields {Uint8Array[]}
 */
async function* withFlushSignalAsync(source) {
  for await (const batch of source) {
    yield batch;
  }
  yield null; // Flush signal
}

/**
 * Convert sync iterable to async iterable.
 * @yields {Uint8Array[]}
 */
async function* syncToAsync(source) {
  for (const item of source) {
    yield item;
  }
}

/**
 * Create an async pipeline from source through transforms.
 * @yields {Uint8Array[]}
 */
async function* createAsyncPipeline(source, transforms, signal) {
  // Check for abort
  if (signal?.aborted) {
    throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
  }

  // Normalize source to async
  let normalized;
  if (isAsyncIterable(source)) {
    normalized = normalizeAsyncSource(source);
  } else if (isSyncIterable(source)) {
    normalized = syncToAsync(normalizeSyncSource(source));
  } else {
    throw new ERR_INVALID_ARG_TYPE('source', ['Iterable', 'AsyncIterable'], source);
  }

  // Fast path: no transforms, just yield normalized source directly
  if (transforms.length === 0) {
    for await (const batch of normalized) {
      if (signal?.aborted) {
        throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
      }
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
      try {
        controller.abort(signal.reason ??
          lazyDOMException('Aborted', 'AbortError'));
      } catch {
        // Transform signal listeners may throw - suppress
      }
    };
    signal.addEventListener('abort', abortHandler, { once: true });
  }

  // Add flush signal
  let current = withFlushSignalAsync(normalized);

  // Apply transforms - each gets the controller's signal
  // Object = stateful, function = stateless
  for (let i = 0; i < transforms.length; i++) {
    const transform = transforms[i];
    const options = { signal: controller.signal };
    if (isTransformObject(transform)) {
      current = applyStatefulAsyncTransform(current, transform.transform,
                                            options);
    } else {
      current = applyStatelessAsyncTransform(current, transform, options);
    }
  }

  // Yield results (filter out null from final output)
  let completed = false;
  try {
    for await (const batch of current) {
      // Check for abort on each iteration
      if (controller.signal.aborted) {
        throw controller.signal.reason ??
              lazyDOMException('Aborted', 'AbortError');
      }
      if (batch !== null) {
        yield batch;
      }
    }
    completed = true;
  } catch (error) {
    if (!controller.signal.aborted) {
      try {
        controller.abort(
          error instanceof Error ? error :
            new ERR_OPERATION_FAILED(String(error)));
      } catch {
        // Transform signal listeners may throw - suppress
      }
    }
    throw error;
  } finally {
    if (!completed && !controller.signal.aborted) {
      try {
        controller.abort(lazyDOMException('Aborted', 'AbortError'));
      } catch {
        // Transform signal listeners may throw - suppress
      }
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
  return {
    *[SymbolIterator]() {
      yield* createSyncPipeline(source, transforms);
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

  return {
    async *[SymbolAsyncIterator]() {
      yield* createAsyncPipeline(source, transforms, options?.signal);
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
  const { transforms, writer, options } = parsePipeToArgs(args);

  // Handle transform-writer
  const finalTransforms = ArrayPrototypeSlice(transforms);
  if (isTransformObject(writer)) {
    ArrayPrototypePush(finalTransforms, writer);
  }

  // Create pipeline
  const pipeline = finalTransforms.length > 0 ?
    createSyncPipeline(
      { [SymbolIterator]: () => source[SymbolIterator]() },
      finalTransforms) :
    source;

  let totalBytes = 0;

  try {
    for (const batch of pipeline) {
      for (let i = 0; i < batch.length; i++) {
        const chunk = batch[i];
        writer.write(chunk);
        totalBytes += chunk.byteLength;
      }
    }

    if (!options?.preventClose) {
      writer.end();
    }
  } catch (error) {
    if (!options?.preventFail) {
      writer.fail(error instanceof Error ? error : new ERR_OPERATION_FAILED(String(error)));
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
  const { transforms, writer, options } = parsePipeToArgs(args);

  // Handle transform-writer
  const finalTransforms = ArrayPrototypeSlice(transforms);
  if (isTransformObject(writer)) {
    ArrayPrototypePush(finalTransforms, writer);
  }

  const signal = options?.signal;

  // Check for abort
  if (signal?.aborted) {
    throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
  }

  let totalBytes = 0;
  const hasWritev = typeof writer.writev === 'function';

  // Helper to write a batch efficiently
  const writeBatch = async (batch) => {
    if (hasWritev && batch.length > 1) {
      await writer.writev(batch, signal ? { signal } : undefined);
      for (let i = 0; i < batch.length; i++) {
        totalBytes += batch[i].byteLength;
      }
    } else {
      const promises = [];
      for (let i = 0; i < batch.length; i++) {
        const chunk = batch[i];
        const result = writer.write(chunk, signal ? { signal } : undefined);
        if (result !== undefined) {
          ArrayPrototypePush(promises, result);
        }
        totalBytes += chunk.byteLength;
      }
      if (promises.length > 0) {
        await SafePromiseAllReturnVoid(promises);
      }
    }
  };

  try {
    // Fast path: no transforms - iterate directly
    if (finalTransforms.length === 0) {
      if (isAsyncIterable(source)) {
        for await (const batch of source) {
          if (signal?.aborted) {
            throw signal.reason ??
                  lazyDOMException('Aborted', 'AbortError');
          }
          await writeBatch(batch);
        }
      } else {
        for (const batch of source) {
          if (signal?.aborted) {
            throw signal.reason ??
                  lazyDOMException('Aborted', 'AbortError');
          }
          await writeBatch(batch);
        }
      }
    } else {
      // Slow path: has transforms - need pipeline
      const streamableSource = isAsyncIterable(source) ?
        { [SymbolAsyncIterator]: () => source[SymbolAsyncIterator]() } :
        { [SymbolIterator]: () => source[SymbolIterator]() };

      const pipeline = createAsyncPipeline(
        streamableSource, finalTransforms, signal);

      for await (const batch of pipeline) {
        if (signal?.aborted) {
          throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
        }
        await writeBatch(batch);
      }
    }

    if (!options?.preventClose) {
      await writer.end(signal ? { signal } : undefined);
    }
  } catch (error) {
    if (!options?.preventFail) {
      await writer.fail(
        error instanceof Error ? error : new ERR_OPERATION_FAILED(String(error)));
    }
    throw error;
  }

  return totalBytes;
}

module.exports = {
  pull,
  pullSync,
  pipeTo,
  pipeToSync,
};
