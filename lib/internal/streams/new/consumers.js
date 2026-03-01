'use strict';

// New Streams API - Consumers & Utilities
//
// bytes(), text(), arrayBuffer() - collect entire stream
// tap(), tapSync() - observe without modifying
// merge() - temporal combining of sources
// ondrain() - backpressure drain utility

const {
  ArrayPrototypeFilter,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  SafePromiseAllReturnVoid,
  SafePromiseRace,
  SymbolAsyncIterator,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const { TextDecoder } = require('internal/encoding');
const { lazyDOMException } = require('internal/util');

const {
  isAsyncIterable,
  isSyncIterable,
} = require('internal/streams/new/from');

const {
  concatBytes,
} = require('internal/streams/new/utils');

const {
  drainableProtocol,
} = require('internal/streams/new/types');

// =============================================================================
// Type Guards
// =============================================================================

function isMergeOptions(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !isAsyncIterable(value) &&
    !isSyncIterable(value)
  );
}

// =============================================================================
// Sync Consumers
// =============================================================================

/**
 * Collect all bytes from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ limit?: number }} [options]
 * @returns {Uint8Array}
 */
function bytesSync(source, options) {
  const limit = options?.limit;
  const chunks = [];
  let totalBytes = 0;

  for (const batch of source) {
    for (let i = 0; i < batch.length; i++) {
      const chunk = batch[i];
      if (limit !== undefined) {
        totalBytes += chunk.byteLength;
        if (totalBytes > limit) {
          throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
        }
      }
      ArrayPrototypePush(chunks, chunk);
    }
  }

  return concatBytes(chunks);
}

/**
 * Collect and decode text from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ encoding?: string, limit?: number }} [options]
 * @returns {string}
 */
function textSync(source, options) {
  const data = bytesSync(source, options);
  const decoder = new TextDecoder(options?.encoding ?? 'utf-8', {
    fatal: true,
    ignoreBOM: true,
  });
  return decoder.decode(data);
}

/**
 * Collect bytes as ArrayBuffer from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ limit?: number }} [options]
 * @returns {ArrayBuffer}
 */
function arrayBufferSync(source, options) {
  const data = bytesSync(source, options);
  if (data.byteOffset === 0 && data.byteLength === data.buffer.byteLength) {
    return data.buffer;
  }
  return data.buffer.slice(data.byteOffset,
                           data.byteOffset + data.byteLength);
}

/**
 * Collect all chunks as an array from a sync source.
 * @param {Iterable<Uint8Array[]>} source
 * @param {{ limit?: number }} [options]
 * @returns {Uint8Array[]}
 */
function arraySync(source, options) {
  const limit = options?.limit;
  const chunks = [];
  let totalBytes = 0;

  for (const batch of source) {
    for (let i = 0; i < batch.length; i++) {
      const chunk = batch[i];
      if (limit !== undefined) {
        totalBytes += chunk.byteLength;
        if (totalBytes > limit) {
          throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
        }
      }
      ArrayPrototypePush(chunks, chunk);
    }
  }

  return chunks;
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
async function bytes(source, options) {
  const signal = options?.signal;
  const limit = options?.limit;

  if (signal?.aborted) {
    throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
  }

  const chunks = [];

  // Fast path: no signal and no limit
  if (!signal && limit === undefined) {
    if (isAsyncIterable(source)) {
      for await (const batch of source) {
        for (let i = 0; i < batch.length; i++) {
          ArrayPrototypePush(chunks, batch[i]);
        }
      }
    } else if (isSyncIterable(source)) {
      for (const batch of source) {
        for (let i = 0; i < batch.length; i++) {
          ArrayPrototypePush(chunks, batch[i]);
        }
      }
    } else {
      throw new ERR_INVALID_ARG_TYPE('source', ['AsyncIterable', 'Iterable'], source);
    }
    return concatBytes(chunks);
  }

  // Slow path: with signal or limit checks
  let totalBytes = 0;

  if (isAsyncIterable(source)) {
    for await (const batch of source) {
      if (signal?.aborted) {
        throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
      }
      for (let i = 0; i < batch.length; i++) {
        const chunk = batch[i];
        if (limit !== undefined) {
          totalBytes += chunk.byteLength;
          if (totalBytes > limit) {
            throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
          }
        }
        ArrayPrototypePush(chunks, chunk);
      }
    }
  } else if (isSyncIterable(source)) {
    for (const batch of source) {
      if (signal?.aborted) {
        throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
      }
      for (let i = 0; i < batch.length; i++) {
        const chunk = batch[i];
        if (limit !== undefined) {
          totalBytes += chunk.byteLength;
          if (totalBytes > limit) {
            throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
          }
        }
        ArrayPrototypePush(chunks, chunk);
      }
    }
  } else {
    throw new ERR_INVALID_ARG_TYPE('source', ['AsyncIterable', 'Iterable'], source);
  }

  return concatBytes(chunks);
}

/**
 * Collect and decode text from an async or sync source.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {{ encoding?: string, signal?: AbortSignal, limit?: number }} [options]
 * @returns {Promise<string>}
 */
async function text(source, options) {
  const data = await bytes(source, options);
  const decoder = new TextDecoder(options?.encoding ?? 'utf-8', {
    fatal: true,
    ignoreBOM: true,
  });
  return decoder.decode(data);
}

/**
 * Collect bytes as ArrayBuffer from an async or sync source.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {{ signal?: AbortSignal, limit?: number }} [options]
 * @returns {Promise<ArrayBuffer>}
 */
async function arrayBuffer(source, options) {
  const data = await bytes(source, options);
  if (data.byteOffset === 0 && data.byteLength === data.buffer.byteLength) {
    return data.buffer;
  }
  return data.buffer.slice(data.byteOffset,
                           data.byteOffset + data.byteLength);
}

/**
 * Collect all chunks as an array from an async or sync source.
 * @param {AsyncIterable<Uint8Array[]>|Iterable<Uint8Array[]>} source
 * @param {{ signal?: AbortSignal, limit?: number }} [options]
 * @returns {Promise<Uint8Array[]>}
 */
async function array(source, options) {
  const signal = options?.signal;
  const limit = options?.limit;

  if (signal?.aborted) {
    throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
  }

  const chunks = [];

  // Fast path: no signal and no limit
  if (!signal && limit === undefined) {
    if (isAsyncIterable(source)) {
      for await (const batch of source) {
        for (let i = 0; i < batch.length; i++) {
          ArrayPrototypePush(chunks, batch[i]);
        }
      }
    } else if (isSyncIterable(source)) {
      for (const batch of source) {
        for (let i = 0; i < batch.length; i++) {
          ArrayPrototypePush(chunks, batch[i]);
        }
      }
    } else {
      throw new ERR_INVALID_ARG_TYPE('source', ['AsyncIterable', 'Iterable'], source);
    }
    return chunks;
  }

  // Slow path
  let totalBytes = 0;

  if (isAsyncIterable(source)) {
    for await (const batch of source) {
      if (signal?.aborted) {
        throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
      }
      for (let i = 0; i < batch.length; i++) {
        const chunk = batch[i];
        if (limit !== undefined) {
          totalBytes += chunk.byteLength;
          if (totalBytes > limit) {
            throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
          }
        }
        ArrayPrototypePush(chunks, chunk);
      }
    }
  } else if (isSyncIterable(source)) {
    for (const batch of source) {
      if (signal?.aborted) {
        throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
      }
      for (let i = 0; i < batch.length; i++) {
        const chunk = batch[i];
        if (limit !== undefined) {
          totalBytes += chunk.byteLength;
          if (totalBytes > limit) {
            throw new ERR_OUT_OF_RANGE('totalBytes', `<= ${limit}`, totalBytes);
          }
        }
        ArrayPrototypePush(chunks, chunk);
      }
    }
  } else {
    throw new ERR_INVALID_ARG_TYPE('source', ['AsyncIterable', 'Iterable'], source);
  }

  return chunks;
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
  return async (chunks) => {
    await callback(chunks);
    return chunks;
  };
}

/**
 * Create a sync pass-through transform that observes chunks.
 * @param {Function} callback
 * @returns {Function}
 */
function tapSync(callback) {
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

  try {
    return drainable[drainableProtocol]();
  } catch {
    return null;
  }
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

  return {
    async *[SymbolAsyncIterator]() {
      const signal = options?.signal;

      if (signal?.aborted) {
        throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
      }

      if (sources.length === 0) return;

      if (sources.length === 1) {
        for await (const batch of sources[0]) {
          if (signal?.aborted) {
            throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
          }
          yield batch;
        }
        return;
      }

      // Multiple sources - race them
      const states = ArrayPrototypeMap(sources, (source) => ({
        iterator: source[SymbolAsyncIterator](),
        done: false,
        pending: null,
      }));

      const startIterator = (state, index) => {
        if (!state.done && !state.pending) {
          state.pending = state.iterator.next().then(
            (result) => ({ index, result }));
        }
      };

      // Start all
      for (let i = 0; i < states.length; i++) {
        startIterator(states[i], i);
      }

      try {
        while (true) {
          if (signal?.aborted) {
            throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
          }

          const pending = ArrayPrototypeFilter(
            ArrayPrototypeMap(states,
                              (state) => state.pending),
            (p) => p !== null);

          if (pending.length === 0) break;

          const { index, result } = await SafePromiseRace(pending);

          states[index].pending = null;

          if (result.done) {
            states[index].done = true;
          } else {
            yield result.value;
            startIterator(states[index], index);
          }
        }
      } finally {
        // Clean up: return all iterators
        await SafePromiseAllReturnVoid(states, async (state) => {
          if (!state.done && state.iterator.return) {
            try {
              await state.iterator.return();
            } catch {
              // Ignore return errors
            }
          }
        });
      }
    },
  };
}

module.exports = {
  bytes,
  bytesSync,
  text,
  textSync,
  arrayBuffer,
  arrayBufferSync,
  array,
  arraySync,
  tap,
  tapSync,
  merge,
  ondrain,
};
