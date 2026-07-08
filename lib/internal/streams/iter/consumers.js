'use strict';

// New Streams API - Consumers & Utilities
//
// bytes(), text(), arrayBuffer() - collect entire stream
// tap(), tapSync() - observe without modifying
// merge() - temporal combining of sources
// ondrain() - backpressure drain utility

const {
  ArrayBufferIsView,
  ArrayBufferPrototypeGetByteLength,
  ArrayBufferPrototypeSlice,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  Promise,
  PromisePrototypeThen,
  SafePromiseAllReturnVoid,
  SymbolAsyncIterator,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { TextDecoder } = require('internal/encoding');
const {
  validateAbortSignal,
  validateFunction,
  validateInteger,
  validateObject,
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
  concatBytes,
  yieldAbortable,
} = require('internal/streams/iter/utils');

const {
  drainableProtocol,
  toAsyncStreamable,
  toStreamable,
} = require('internal/streams/iter/types');

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
  let totalBytes = 0;

  for (const batch of normalized) {
    for (let i = 0; i < batch.length; i++) {
      const chunk = batch[i];
      if (limit !== undefined) {
        totalBytes += TypedArrayPrototypeGetByteLength(chunk);
        if (totalBytes > limit) {
          throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
        }
      }
      ArrayPrototypePush(chunks, chunk);
    }
  }

  return chunks;
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
  const abortableSource = signal && isAsyncIterable(source) ?
    yieldAbortable(source, signal) : source;
  const normalized = from(abortableSource);
  const chunks = [];

  // Fast path: no signal and no limit
  if (!signal && limit === undefined) {
    for await (const batch of normalized) {
      for (let i = 0; i < batch.length; i++) {
        ArrayPrototypePush(chunks, batch[i]);
      }
    }
    return chunks;
  }

  // Slow path: with signal or limit checks
  let totalBytes = 0;
  const iterable = signal ? yieldAbortable(normalized, signal) : normalized;

  for await (const batch of iterable) {
    signal?.throwIfAborted();
    for (let i = 0; i < batch.length; i++) {
      const chunk = batch[i];
      if (limit !== undefined) {
        totalBytes += TypedArrayPrototypeGetByteLength(chunk);
        if (totalBytes > limit) {
          throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
        }
      }
      ArrayPrototypePush(chunks, chunk);
    }
  }

  return chunks;
}

/**
 * Convert a Uint8Array to its backing ArrayBuffer, slicing if necessary.
 * Handles both ArrayBuffer and SharedArrayBuffer backing stores.
 * @param {Uint8Array} data
 * @returns {ArrayBuffer|SharedArrayBuffer}
 */
function toArrayBuffer(data) {
  const byteOffset = TypedArrayPrototypeGetByteOffset(data);
  const byteLength = TypedArrayPrototypeGetByteLength(data);
  const buffer = TypedArrayPrototypeGetBuffer(data);
  // SharedArrayBuffer is not available in primordials, so use
  // direct property access for its byteLength and slice.
  if (isSharedArrayBuffer(buffer)) {
    if (byteOffset === 0 && byteLength === buffer.byteLength) {
      return buffer;
    }
    return buffer.slice(byteOffset, byteOffset + byteLength);
  }
  if (byteOffset === 0 &&
      byteLength === ArrayBufferPrototypeGetByteLength(buffer)) {
    return buffer;
  }
  return ArrayBufferPrototypeSlice(buffer, byteOffset,
                                   byteOffset + byteLength);
}

// =============================================================================
// Shared option validation
// =============================================================================

function validateBaseConsumerOptions(options) {
  validateObject(options, 'options');
  if (options.limit !== undefined) {
    validateInteger(options.limit, 'options.limit', 0);
  }
  if (options.encoding !== undefined) {
    if (typeof options.encoding !== 'string') {
      throw new ERR_INVALID_ARG_TYPE('options.encoding', 'string',
                                     options.encoding);
    }
    try {
      new TextDecoder(options.encoding);
    } catch {
      throw new ERR_INVALID_ARG_VALUE.RangeError(
        'options.encoding', options.encoding);
    }
  }
}

function validateConsumerOptions(options) {
  validateBaseConsumerOptions(options);
  if (options.signal !== undefined) {
    validateAbortSignal(options.signal, 'options.signal');
  }
}

function validateSyncConsumerOptions(options) {
  validateBaseConsumerOptions(options);
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
  validateSyncConsumerOptions(options);
  return concatBytes(collectSync(source, options.limit));
}

/**
 * Collect and decode text from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ encoding?: string, limit?: number }} [options]
 * @returns {string}
 */
function textSync(source, options = kNullPrototype) {
  validateSyncConsumerOptions(options);
  const data = concatBytes(collectSync(source, options.limit));
  const decoder = new TextDecoder(options.encoding ?? 'utf-8', {
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
  validateSyncConsumerOptions(options);
  return toArrayBuffer(concatBytes(collectSync(source, options.limit)));
}

/**
 * Collect all chunks as an array from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ limit?: number }} [options]
 * @returns {Uint8Array[]}
 */
function arraySync(source, options = kNullPrototype) {
  validateSyncConsumerOptions(options);
  return collectSync(source, options.limit);
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
  validateConsumerOptions(options);
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
  validateConsumerOptions(options);
  const chunks = await collectAsync(source, options.signal, options.limit);
  const data = concatBytes(chunks);
  const decoder = new TextDecoder(options.encoding ?? 'utf-8', {
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
  validateConsumerOptions(options);
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
  validateConsumerOptions(options);
  return collectAsync(source, options.signal, options.limit);
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

  if (
    !(drainableProtocol in drainable) ||
    typeof drainable[drainableProtocol] !== 'function'
  ) {
    return null;
  }

  return drainable[drainableProtocol]();
}

// =============================================================================
// Merge Utility
// =============================================================================

/**
 * Merge multiple async iterables by yielding values in temporal order.
 * @param {...(AsyncIterable<Uint8Array[]>|object)} args
 * @returns {AsyncIterable<Uint8Array[]>}
 */
function merge(...args) {
  let sources;
  let options;

  if (args.length > 0 && isMergeOptions(args[args.length - 1])) {
    options = args[args.length - 1];
    sources = ArrayPrototypeSlice(args, 0, -1);
  } else {
    sources = args;
  }

  if (options?.signal !== undefined) {
    validateAbortSignal(options.signal, 'options.signal');
  }

  // Normalize each source via from()
  const normalized = ArrayPrototypeMap(sources, (source) => from(source));

  return {
    __proto__: null,
    async *[SymbolAsyncIterator]() {
      const signal = options?.signal;

      signal?.throwIfAborted();

      if (normalized.length === 0) return;

      if (normalized.length === 1) {
        for await (const batch of normalized[0]) {
          signal?.throwIfAborted();
          yield batch;
        }
        return;
      }

      // Multiple sources - use a ready queue so that batches that settle
      // between consumer pulls are drained synchronously without an extra
      // async tick per batch. Each source has at most one pending .next()
      // at a time. Every batch from every source is preserved.
      const ready = [];
      let activeCount = normalized.length;
      let waitResolve = null;
      let onAbort;

      if (signal) {
        onAbort = () => {
          if (waitResolve) {
            waitResolve();
            waitResolve = null;
          }
        };
        signal.addEventListener('abort', onAbort, {
          __proto__: null,
          once: true,
        });
      }

      // Called when a source's .next() settles. Pushes the result into
      // the ready queue and wakes the consumer if it's waiting.
      const onSettled = (iterator, result) => {
        if (result.done) {
          activeCount--;
        } else {
          ArrayPrototypePush(ready, {
            __proto__: null,
            iterator,
            value: result.value,
          });
        }
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
        PromisePrototypeThen(
          iterator.next(),
          (r) => onSettled(iterator, r),
          (err) => {
            ArrayPrototypePush(ready, { __proto__: null, error: err });
            if (waitResolve) {
              waitResolve();
              waitResolve = null;
            }
          },
        );
      }

      let primaryError;
      try {
        while (activeCount > 0 || ready.length > 0) {
          signal?.throwIfAborted();

          // Drain ready queue synchronously
          while (ready.length > 0) {
            const item = ArrayPrototypeShift(ready);
            if (item?.error) {
              throw item.error;
            }
            yield item.value;
            PromisePrototypeThen(
              item.iterator.next(),
              (r) => onSettled(item.iterator, r),
              (err) => {
                ArrayPrototypePush(ready, { __proto__: null, error: err });
                if (waitResolve) {
                  waitResolve();
                  waitResolve = null;
                }
              },
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
        if (onAbort !== undefined) {
          signal.removeEventListener('abort', onAbort);
        }
        // Clean up: return all iterators. Cleanup errors are not
        // swallowed - a broken iterator.return() (e.g., failing to
        // release a resource) should be visible to the caller.
        await cleanupIterators(
          iterators,
          primaryError,
          signal?.aborted && primaryError === signal.reason,
        );
      }
    },
  };
}

async function cleanupIterators(iterators, primaryError, skipAwaitCleanup) {
  let cleanupError;
  await SafePromiseAllReturnVoid(iterators, async (iterator) => {
    if (iterator.return) {
      try {
        const result = iterator.return();
        if (skipAwaitCleanup) {
          markPromiseAsHandled(result);
        } else {
          await result;
        }
      } catch (err) {
        // Keep the first cleanup error encountered.
        cleanupError ??= err;
      }
    }
  });
  if (cleanupError !== undefined) {
    if (primaryError !== undefined) {
      // Both a primary error and a cleanup error occurred.
      // Wrap in SuppressedError so neither is lost:
      // .error = primaryError, .suppressed = cleanupError.
      // eslint-disable-next-line no-restricted-syntax
      throw new SuppressedError(primaryError, cleanupError);
    }
    // No primary error - the cleanup error is the only error.
    throw cleanupError;
  }
  if (primaryError !== undefined) {
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
  merge,
  ondrain,
  tap,
  tapSync,
  text,
  textSync,
};
