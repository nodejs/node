'use strict';

// New Streams API - Consumers & Utilities
//
// bytes(), text(), arrayBuffer() - collect entire stream
// dump() - consume entire stream, retaining nothing
// tap(), tapSync() - observe without modifying
// merge() - temporal combining of sources
// ondrain() - backpressure drain utility

const {
  ArrayBufferIsView,
  ArrayBufferPrototypeGetByteLength,
  ArrayBufferPrototypeSlice,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  FunctionPrototypeCall,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  SafePromiseAllReturnVoid,
  SafeSet,
  Symbol,
  SymbolAsyncIterator,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  TypedArrayPrototypeSet,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { TextDecoder } = require('internal/encoding');
const { RingBuffer } = require('internal/streams/iter/ringbuffer');
const {
  validateFunction,
} = require('internal/validators');

const {
  markPromiseAsHandled,
} = internalBinding('util');

const {
  from,
  fromSync,
  isAsyncIterable,
  isSyncIterable,
} = require('internal/streams/iter/from');

const {
  IterResult,
  kNullOnceOption,
  concatBytes,
  getProtocolMethod,
  raceSignal,
  recordChunk,
  validateRecordedChunks,
  yieldAbortable,
} = require('internal/streams/iter/utils');

const {
  drainableProtocol,
  toAsyncStreamable,
  toStreamable,
} = require('internal/streams/iter/types');
const {
  converters,
} = require('internal/streams/iter/webidl');

const {
  isAnyArrayBuffer,
  isSharedArrayBuffer,
} = require('internal/util/types');

// =============================================================================
// Type Guards
// =============================================================================

function isMergeOptions(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !ArrayBufferIsView(value) &&
    typeof value[toStreamable] !== 'function' &&
    typeof value[toAsyncStreamable] !== 'function' &&
    !isAsyncIterable(value) &&
    !isSyncIterable(value) &&
    !isAnyArrayBuffer(value)
  );
}

// =============================================================================
// Shared chunk collection helpers
// =============================================================================

/**
 * Collect chunks from a sync source into an array.
 * @param {Iterable<Uint8Array[]>} source
 * @param {number} [limit]
 * @returns {Uint8Array[]}
 */
function collectSync(source, limit) {
  // Normalize source via fromSync() - accepts strings, ArrayBuffers, protocols, etc.
  const normalized = fromSync(source);
  const chunks = [];
  const byteLengths = [];
  let totalBytes = 0;

  for (const batch of normalized) {
    for (let i = 0; i < batch.length; i++) {
      totalBytes += recordChunk(chunks, byteLengths, batch[i]);
      if (limit !== undefined && totalBytes > limit) {
        throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
      }
    }
  }

  return validateRecordedChunks(chunks, byteLengths);
}

/**
 * Collect chunks from an async or sync source into an array.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {AbortSignal} [signal]
 * @param {number} [limit]
 * @returns {Promise<Uint8Array[]>}
 */
async function collectAsync(source, signal, limit) {
  signal?.throwIfAborted();

  // Normalize source via from() - accepts strings, ArrayBuffers, protocols, etc.
  const normalized = from(source);
  const chunks = [];
  const byteLengths = [];

  // Fast path: no signal and no limit
  if (!signal && limit === undefined) {
    for await (const batch of normalized) {
      for (let i = 0; i < batch.length; i++) {
        recordChunk(chunks, byteLengths, batch[i]);
      }
    }
    return validateRecordedChunks(chunks, byteLengths);
  }

  // Slow path: with signal or limit byteLengths
  let totalBytes = 0;
  function recordBatch(batch) {
    for (let i = 0; i < batch.length; i++) {
      totalBytes += recordChunk(chunks, byteLengths, batch[i]);
      if (limit !== undefined && totalBytes > limit) {
        throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
      }
    }
  }

  if (!signal) {
    for await (const batch of normalized) recordBatch(batch);
    return validateRecordedChunks(chunks, byteLengths);
  }

  // Read as for await...of does, stopping once the signal aborts; see
  // raceSignal().
  const state = { __proto__: null, aborted: false };
  async function consume(iterator) {
    for (;;) {
      const result = await iterator.next();
      if (result.done || state.aborted) return;
      try {
        recordBatch(result.value);
      } catch (error) {
        // As for await closes the source when its body throws, ignoring
        // errors from closing it.
        try {
          await iterator.return?.();
        } catch {
          // The error recording the batch is thrown.
        }
        throw error;
      }
    }
  }
  await raceSignal(normalized[SymbolAsyncIterator](), signal, state, consume);
  return validateRecordedChunks(chunks, byteLengths);
}

/**
 * Convert a Uint8Array to its backing ArrayBuffer, slicing if necessary.
 * @param {Uint8Array} data
 * @returns {ArrayBuffer}
 */
function toArrayBuffer(data) {
  const byteOffset = TypedArrayPrototypeGetByteOffset(data);
  const byteLength = TypedArrayPrototypeGetByteLength(data);
  const buffer = TypedArrayPrototypeGetBuffer(data);
  if (isSharedArrayBuffer(buffer)) {
    const copy = new Uint8Array(byteLength);
    TypedArrayPrototypeSet(copy, data);
    return TypedArrayPrototypeGetBuffer(copy);
  }
  if (byteOffset === 0 &&
      byteLength === ArrayBufferPrototypeGetByteLength(buffer)) {
    return buffer;
  }
  return ArrayBufferPrototypeSlice(buffer, byteOffset,
                                   byteOffset + byteLength);
}

// =============================================================================
// Sync Consumers
// =============================================================================

const kNullPrototype = { __proto__: null };

/**
 * Collect all bytes from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ limit?: number }} [options]
 * @returns {Uint8Array}
 */
function bytesSync(source, options = kNullPrototype) {
  options = converters.ConsumeSyncOptions(options, {
    __proto__: null,
    context: 'options',
  });
  return concatBytes(collectSync(source, options.limit));
}

/**
 * Collect and decode text from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ encoding?: string, limit?: number }} [options]
 * @returns {string}
 */
function textSync(source, options = kNullPrototype) {
  options = converters.TextConsumeSyncOptions(options, {
    __proto__: null,
    context: 'options',
  });
  try {
    new TextDecoder(options.encoding);
  } catch {
    throw new ERR_INVALID_ARG_VALUE.RangeError(
      'options.encoding', options.encoding);
  }
  const data = concatBytes(collectSync(source, options.limit));
  const decoder = new TextDecoder(options.encoding, {
    __proto__: null,
    fatal: true,
  });
  return decoder.decode(data);
}

/**
 * Collect bytes as ArrayBuffer from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ limit?: number }} [options]
 * @returns {ArrayBuffer}
 */
function arrayBufferSync(source, options = kNullPrototype) {
  options = converters.ConsumeSyncOptions(options, {
    __proto__: null,
    context: 'options',
  });
  return toArrayBuffer(concatBytes(collectSync(source, options.limit)));
}

/**
 * Collect all chunks as an array from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ limit?: number }} [options]
 * @returns {Uint8Array[]}
 */
function arraySync(source, options = kNullPrototype) {
  options = converters.ConsumeSyncOptions(options, {
    __proto__: null,
    context: 'options',
  });
  return collectSync(source, options.limit);
}

/**
 * Read a sync source to completion, discarding every chunk.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ limit?: number }} [options]
 * @returns {undefined}
 */
function dumpSync(source, options = kNullPrototype) {
  options = converters.ConsumeSyncOptions(options, {
    __proto__: null,
    context: 'options',
  });

  const limit = options.limit;
  const normalized = fromSync(source);
  let totalBytes = 0;

  for (const batch of normalized) {
    // With no limit, just iterate through the stream completely.
    if (limit === undefined) continue;
    // Otherwise, calculate totalBytes and track the limit.
    for (let i = 0; i < batch.length; i++) {
      totalBytes += TypedArrayPrototypeGetByteLength(batch[i]);
      if (totalBytes > limit) {
        throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
      }
    }
  }
}

// =============================================================================
// Async Consumers
// =============================================================================

/**
 * Collect all bytes from an async or sync source.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {{ signal?: AbortSignal, limit?: number }} [options]
 * @returns {Promise<Uint8Array>}
 */
async function bytes(source, options = kNullPrototype) {
  options = converters.ConsumeOptions(options, {
    __proto__: null,
    context: 'options',
  });
  const chunks = await collectAsync(source, options.signal, options.limit);
  return concatBytes(chunks);
}

/**
 * Collect and decode text from an async or sync source.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {{ encoding?: string, signal?: AbortSignal, limit?: number }} [options]
 * @returns {Promise<string>}
 */
async function text(source, options = kNullPrototype) {
  options = converters.TextConsumeOptions(options, {
    __proto__: null,
    context: 'options',
  });
  try {
    new TextDecoder(options.encoding);
  } catch {
    throw new ERR_INVALID_ARG_VALUE.RangeError(
      'options.encoding', options.encoding);
  }
  const chunks = await collectAsync(source, options.signal, options.limit);
  const data = concatBytes(chunks);
  const decoder = new TextDecoder(options.encoding, {
    __proto__: null,
    fatal: true,
  });
  return decoder.decode(data);
}

/**
 * Collect bytes as ArrayBuffer from an async or sync source.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {{ signal?: AbortSignal, limit?: number }} [options]
 * @returns {Promise<ArrayBuffer>}
 */
async function arrayBuffer(source, options = kNullPrototype) {
  options = converters.ConsumeOptions(options, {
    __proto__: null,
    context: 'options',
  });
  const chunks = await collectAsync(source, options.signal, options.limit);
  return toArrayBuffer(concatBytes(chunks));
}

/**
 * Collect all chunks as an array from an async or sync source.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {{ signal?: AbortSignal, limit?: number }} [options]
 * @returns {Promise<Uint8Array[]>}
 */
async function array(source, options = kNullPrototype) {
  options = converters.ConsumeOptions(options, {
    __proto__: null,
    context: 'options',
  });
  return collectAsync(source, options.signal, options.limit);
}

/**
 * Read an async or sync source to completion, discarding every chunk.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {{ signal?: AbortSignal, limit?: number }} [options]
 * @returns {Promise<undefined>}
 */
async function dump(source, options = kNullPrototype) {
  options = converters.ConsumeOptions(options, {
    __proto__: null,
    context: 'options',
  });
  const signal = options.signal;
  const limit = options.limit;

  signal?.throwIfAborted();

  const abortableSource = signal && isAsyncIterable(source) ?
    yieldAbortable(source, signal) : source;
  const normalized = from(abortableSource);
  const iterable = signal ? yieldAbortable(normalized, signal) : normalized;

  let totalBytes = 0;

  for await (const batch of iterable) {
    signal?.throwIfAborted();
    // With no limit, just iterate through the stream completely.
    if (limit === undefined) continue;
    // Otherwise, calculate totalBytes and track the limit.
    for (let i = 0; i < batch.length; i++) {
      totalBytes += TypedArrayPrototypeGetByteLength(batch[i]);
      if (totalBytes > limit) {
        throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
      }
    }
  }
}

// =============================================================================
// Tap Utilities
// =============================================================================

/**
 * Create a pass-through transform that observes chunks without modifying them.
 * @param {Function} callback
 * @returns {Function}
 */
function tap(callback) {
  validateFunction(callback, 'callback');
  return async (chunks, options) => {
    await callback(chunks, options);
    return chunks;
  };
}

/**
 * Create a sync pass-through transform that observes chunks.
 * @param {Function} callback
 * @returns {Function}
 */
function tapSync(callback) {
  validateFunction(callback, 'callback');
  return (chunks) => {
    callback(chunks);
    return chunks;
  };
}

// =============================================================================
// Drain Utility
// =============================================================================

/**
 * Wait for a drainable object's backpressure to clear.
 * @param {object} drainable
 * @returns {Promise<boolean>|null}
 */
function ondrain(drainable) {
  if (
    drainable === null ||
    drainable === undefined ||
    typeof drainable !== 'object'
  ) {
    return null;
  }

  const protocol = getProtocolMethod(drainable, drainableProtocol);
  return protocol === undefined ?
    null : FunctionPrototypeCall(protocol, drainable);
}

// =============================================================================
// Merge Utility
// =============================================================================

const kNoMergeError = Symbol('kNoMergeError');

// An entry in merge()'s ready queue: a value from `iterator`, or, for a
// source that failed, `reason` with no iterator. Created for every merged
// chunk, so constructed rather than created as a dictionary-mode literal.
function MergeEntry(iterator, value, reason) {
  this.iterator = iterator;
  this.value = value;
  this.reason = reason;
}
MergeEntry.prototype = ObjectFreeze({ __proto__: null });

/**
 * Merge multiple async iterables by yielding values in temporal order.
 * @param {...(AsyncIterable<Uint8Array[]>|object)} args
 * @returns {AsyncIterable<Uint8Array[]>}
 */
/**
 * The iterator of merge() of a single source without a signal: what an
 * async generator doing `for await (const batch of source) yield batch;`
 * gives, without its layer for every batch. Calls to next() are passed to
 * the source's iterator, from from(), which queues them as an async
 * generator does. return() and throw() wait for the last next() before
 * closing the source, as the generator would queue them, and later calls
 * wait for them in turn. The source is opened by the first next().
 */
class MergeSourceIterator {
  #source;
  #iterator = null;
  // The last next() of the source's iterator, and the promise of return()
  // or throw() once one is called.
  #last = null;
  #closing = null;

  constructor(source) {
    this.#source = source;
  }

  next() {
    if (this.#closing !== null) {
      return PromisePrototypeThen(this.#closing, doneResult, doneResult);
    }
    try {
      this.#iterator ??= this.#source[SymbolAsyncIterator]();
      const next = PromiseResolve(this.#iterator.next());
      this.#last = next;
      return next;
    } catch (error) {
      this.#closing = PromiseReject(error);
      markPromiseAsHandled(this.#closing);
      return this.#closing;
    }
  }

  return(value) {
    return this.#close(() => new IterResult(true, value), false);
  }

  throw(error) {
    return this.#close(() => { throw error; }, true);
  }

  // Close the source once the last next() has settled, if it was opened,
  // then settle with `settle()`. Errors closing it are ignored if `quiet`,
  // as for await ignores them when its body throws.
  #close(settle, quiet) {
    if (this.#closing !== null) {
      return PromisePrototypeThen(this.#closing, settle, settle);
    }
    const iterator = this.#iterator;
    if (iterator === null) {
      this.#closing = PromiseResolve();
      return PromisePrototypeThen(this.#closing, settle);
    }
    const close = () => {
      const closed = PromiseResolve(iterator.return?.());
      return quiet ? PromisePrototypeThen(closed, undefined, () => {}) : closed;
    };
    const closing = PromisePrototypeThen(this.#last ?? PromiseResolve(),
                                         close, close);
    this.#closing = closing;
    markPromiseAsHandled(closing);
    return PromisePrototypeThen(closing, settle);
  }

  [SymbolAsyncIterator]() {
    return this;
  }
}
ObjectSetPrototypeOf(MergeSourceIterator.prototype, null);

function doneResult() {
  return new IterResult(true, undefined);
}

function merge(...args) {
  let sources;
  let options;

  if (args.length > 0 && isMergeOptions(args[args.length - 1])) {
    options = args[args.length - 1];
    sources = ArrayPrototypeSlice(args, 0, -1);
  } else {
    sources = args;
  }

  options = converters.MergeOptions(options, {
    __proto__: null,
    context: 'options',
  });

  // Normalize each source via from()
  const normalized = ArrayPrototypeMap(sources, (source) => from(source));

  if (normalized.length === 1) {
    // A single source is read without an async generator layer for every
    // batch (see MergeSourceIterator). With a signal, the source is read through
    // yieldAbortable(), which rejects a pending read when the signal aborts
    // (also before the first read), closes the source, and then ends. An
    // async iterable is made abortable before from(), so that an abort
    // closes it even while from() is reading it.
    const { signal } = options;
    return {
      __proto__: null,
      [SymbolAsyncIterator]() {
        if (signal === undefined) {
          return new MergeSourceIterator(normalized[0]);
        }
        const source = isAsyncIterable(sources[0]) ?
          from(yieldAbortable(sources[0], signal)) :
          yieldAbortable(normalized[0], signal);
        return source[SymbolAsyncIterator]();
      },
    };
  }

  return {
    __proto__: null,
    async *[SymbolAsyncIterator]() {
      const { signal } = options;

      signal?.throwIfAborted();

      if (normalized.length === 0) return;

      // Multiple sources - use a ready queue so that batches that settle
      // between consumer pulls are drained synchronously without an extra
      // async tick per batch. Each source has at most one pending .next()
      // at a time. Every batch from every source is preserved.
      const ready = new RingBuffer();
      const pendingPulls = new SafeSet();
      let activeCount = normalized.length;
      let waitResolve = null;
      let onAbort;
      let stopped = false;

      if (signal) {
        onAbort = () => {
          if (waitResolve) {
            waitResolve();
            waitResolve = null;
          }
        };
        signal.addEventListener('abort', onAbort, kNullOnceOption);
      }

      // Called when a source's .next() settles. Pushes the result into
      // the ready queue and wakes the consumer if it's waiting.
      const onSettled = (iterator, result) => {
        pendingPulls.delete(iterator);
        if (stopped) return;
        if (result.done) {
          activeCount--;
        } else {
          ready.push(new MergeEntry(iterator, result.value, undefined));
        }
        if (waitResolve) {
          waitResolve();
          waitResolve = null;
        }
      };

      const onRejected = (iterator, reason) => {
        pendingPulls.delete(iterator);
        if (stopped) return;
        ready.push(new MergeEntry(undefined, undefined, reason));
        if (waitResolve) {
          waitResolve();
          waitResolve = null;
        }
      };

      // Start one .next() per source
      const iterators = [];
      for (let i = 0; i < normalized.length; i++) {
        const iterator = normalized[i][SymbolAsyncIterator]();
        ArrayPrototypePush(iterators, iterator);
        pendingPulls.add(iterator);
        PromisePrototypeThen(
          iterator.next(),
          (r) => onSettled(iterator, r),
          (reason) => onRejected(iterator, reason),
        );
      }

      let primaryError = kNoMergeError;
      try {
        while (activeCount > 0 || ready.length > 0) {
          signal?.throwIfAborted();

          // Drain ready queue synchronously
          while (ready.length > 0) {
            const item = ready.shift();
            if (item.iterator === undefined) {
              throw item.reason;
            }
            yield item.value;
            pendingPulls.add(item.iterator);
            PromisePrototypeThen(
              item.iterator.next(),
              (r) => onSettled(item.iterator, r),
              (reason) => onRejected(item.iterator, reason),
            );
          }

          // If sources are still active, wait for the next settlement
          if (activeCount > 0) {
            await new Promise((resolve) => {
              waitResolve = resolve;
              if (signal?.aborted) {
                waitResolve = null;
                resolve();
              }
            });
          }
        }
      } catch (err) {
        primaryError = err;
      } finally {
        stopped = true;
        if (onAbort !== undefined) {
          signal.removeEventListener('abort', onAbort);
        }
        // Clean up: return all iterators. Cleanup errors are not
        // swallowed - a broken iterator.return() (e.g., failing to
        // release a resource) should be visible to the caller.
        await cleanupIterators(
          iterators,
          primaryError,
          pendingPulls,
        );
      }
    },
  };
}

async function cleanupIterators(iterators, primaryError, pendingPulls) {
  let cleanupError = kNoMergeError;
  await SafePromiseAllReturnVoid(iterators, async (iterator) => {
    if (iterator.return) {
      try {
        const result = iterator.return();
        if (pendingPulls.has(iterator)) {
          markPromiseAsHandled(result);
        } else {
          await result;
        }
      } catch (err) {
        // Keep the first cleanup error encountered.
        if (cleanupError === kNoMergeError) cleanupError = err;
      }
    }
  });
  if (cleanupError !== kNoMergeError) {
    if (primaryError !== kNoMergeError) {
      // Both a primary error and a cleanup error occurred.
      // Wrap in SuppressedError so neither is lost:
      // .error = primaryError, .suppressed = cleanupError.
      // eslint-disable-next-line no-restricted-syntax
      throw new SuppressedError(primaryError, cleanupError);
    }
    // No primary error - the cleanup error is the only error.
    throw cleanupError;
  }
  if (primaryError !== kNoMergeError) {
    throw primaryError;
  }
}

module.exports = {
  array,
  arrayBuffer,
  arrayBufferSync,
  arraySync,
  bytes,
  bytesSync,
  dump,
  dumpSync,
  merge,
  ondrain,
  tap,
  tapSync,
  text,
  textSync,
};
