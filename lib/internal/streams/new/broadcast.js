'use strict';

// New Streams API - Broadcast
//
// Push-model multi-consumer streaming. A single writer can push data to
// multiple consumers. Each consumer has an independent cursor into a
// shared buffer.

const {
  ArrayIsArray,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  Error,
  MathMax,
  Promise,
  PromiseResolve,
  SafeSet,
  String,
  SymbolAsyncIterator,
  SymbolDispose,
} = primordials;

const { TextEncoder } = require('internal/encoding');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const {
  broadcastProtocol,
  drainableProtocol,
} = require('internal/streams/new/types');

const {
  isAsyncIterable,
  isSyncIterable,
} = require('internal/streams/new/from');

const {
  pull: pullWithTransforms,
} = require('internal/streams/new/pull');

const encoder = new TextEncoder();

// =============================================================================
// Argument Parsing
// =============================================================================

function isPushStreamOptions(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    !('transform' in value) &&
    !('write' in value)
  );
}

function parsePushArgs(args) {
  if (args.length === 0) {
    return { transforms: [], options: undefined };
  }
  const last = args[args.length - 1];
  if (isPushStreamOptions(last)) {
    return {
      transforms: ArrayPrototypeSlice(args, 0, -1),
      options: last,
    };
  }
  return { transforms: args, options: undefined };
}

// =============================================================================
// Broadcast Implementation
// =============================================================================

class BroadcastImpl {
  constructor(options) {
    this._buffer = [];
    this._bufferStart = 0;
    this._consumers = new SafeSet();
    this._ended = false;
    this._error = null;
    this._cancelled = false;
    this._options = options;
    this._onBufferDrained = null;
  }

  get consumerCount() {
    return this._consumers.size;
  }

  get bufferSize() {
    return this._buffer.length;
  }

  push(...args) {
    const { transforms, options } = parsePushArgs(args);
    const rawConsumer = this._createRawConsumer();

    if (transforms.length > 0) {
      if (options?.signal) {
        return pullWithTransforms(
          rawConsumer, ...transforms, { signal: options.signal });
      }
      return pullWithTransforms(rawConsumer, ...transforms);
    }
    return rawConsumer;
  }

  _createRawConsumer() {
    const state = {
      cursor: this._bufferStart + this._buffer.length,
      resolve: null,
      reject: null,
      detached: false,
    };

    this._consumers.add(state);
    const self = this;

    return {
      [SymbolAsyncIterator]() {
        return {
          async next() {
            if (state.detached) {
              return { __proto__: null, done: true, value: undefined };
            }

            const bufferIndex = state.cursor - self._bufferStart;
            if (bufferIndex < self._buffer.length) {
              const chunk = self._buffer[bufferIndex];
              state.cursor++;
              self._tryTrimBuffer();
              return { __proto__: null, done: false, value: chunk };
            }

            if (self._error) {
              state.detached = true;
              self._consumers.delete(state);
              throw self._error;
            }

            if (self._ended || self._cancelled) {
              state.detached = true;
              self._consumers.delete(state);
              return { __proto__: null, done: true, value: undefined };
            }

            return new Promise((resolve, reject) => {
              state.resolve = resolve;
              state.reject = reject;
            });
          },

          async return() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            self._consumers.delete(state);
            self._tryTrimBuffer();
            return { __proto__: null, done: true, value: undefined };
          },

          async throw() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            self._consumers.delete(state);
            self._tryTrimBuffer();
            return { __proto__: null, done: true, value: undefined };
          },
        };
      },
    };
  }

  cancel(reason) {
    if (this._cancelled) return;
    this._cancelled = true;

    if (reason) {
      this._error = reason;
    }

    for (const consumer of this._consumers) {
      if (consumer.resolve) {
        if (reason) {
          consumer.reject?.(reason);
        } else {
          consumer.resolve({ done: true, value: undefined });
        }
        consumer.resolve = null;
        consumer.reject = null;
      }
      consumer.detached = true;
    }
    this._consumers.clear();
  }

  [SymbolDispose]() {
    this.cancel();
  }

  // Internal methods called by Writer

  _write(chunk) {
    if (this._ended || this._cancelled) return false;

    if (this._buffer.length >= this._options.highWaterMark) {
      switch (this._options.backpressure) {
        case 'strict':
        case 'block':
          return false;
        case 'drop-oldest':
          ArrayPrototypeShift(this._buffer);
          this._bufferStart++;
          for (const consumer of this._consumers) {
            if (consumer.cursor < this._bufferStart) {
              consumer.cursor = this._bufferStart;
            }
          }
          break;
        case 'drop-newest':
          return true;
      }
    }

    ArrayPrototypePush(this._buffer, chunk);
    this._notifyConsumers();
    return true;
  }

  _end() {
    if (this._ended) return;
    this._ended = true;

    for (const consumer of this._consumers) {
      if (consumer.resolve) {
        const bufferIndex = consumer.cursor - this._bufferStart;
        if (bufferIndex < this._buffer.length) {
          const chunk = this._buffer[bufferIndex];
          consumer.cursor++;
          consumer.resolve({ done: false, value: chunk });
        } else {
          consumer.resolve({ done: true, value: undefined });
        }
        consumer.resolve = null;
        consumer.reject = null;
      }
    }
  }

  _abort(reason) {
    if (this._ended || this._error) return;
    this._error = reason;
    this._ended = true;

    for (const consumer of this._consumers) {
      if (consumer.reject) {
        consumer.reject(reason);
        consumer.resolve = null;
        consumer.reject = null;
      }
    }
  }

  _getDesiredSize() {
    if (this._ended || this._cancelled) return null;
    return MathMax(0, this._options.highWaterMark - this._buffer.length);
  }

  _canWrite() {
    if (this._ended || this._cancelled) return false;
    if ((this._options.backpressure === 'strict' ||
         this._options.backpressure === 'block') &&
        this._buffer.length >= this._options.highWaterMark) {
      return false;
    }
    return true;
  }

  _getMinCursor() {
    let min = Infinity;
    for (const consumer of this._consumers) {
      if (consumer.cursor < min) {
        min = consumer.cursor;
      }
    }
    return min === Infinity ?
      this._bufferStart + this._buffer.length : min;
  }

  _tryTrimBuffer() {
    const minCursor = this._getMinCursor();
    const trimCount = minCursor - this._bufferStart;
    if (trimCount > 0) {
      ArrayPrototypeSplice(this._buffer, 0, trimCount);
      this._bufferStart = minCursor;

      if (this._onBufferDrained &&
          this._buffer.length < this._options.highWaterMark) {
        this._onBufferDrained();
      }
    }
  }

  _notifyConsumers() {
    for (const consumer of this._consumers) {
      if (consumer.resolve) {
        const bufferIndex = consumer.cursor - this._bufferStart;
        if (bufferIndex < this._buffer.length) {
          const chunk = this._buffer[bufferIndex];
          consumer.cursor++;
          const resolve = consumer.resolve;
          consumer.resolve = null;
          consumer.reject = null;
          resolve({ done: false, value: chunk });
          this._tryTrimBuffer();
        }
      }
    }
  }
}

// =============================================================================
// BroadcastWriter
// =============================================================================

class BroadcastWriter {
  constructor(broadcastImpl) {
    this._broadcast = broadcastImpl;
    this._totalBytes = 0;
    this._closed = false;
    this._aborted = false;
    this._pendingWrites = [];
    this._pendingDrains = [];

    this._broadcast._onBufferDrained = () => {
      this._resolvePendingWrites();
      this._resolvePendingDrains(true);
    };
  }

  [drainableProtocol]() {
    const desired = this.desiredSize;
    if (desired === null) return null;
    if (desired > 0) return PromiseResolve(true);
    return new Promise((resolve, reject) => {
      ArrayPrototypePush(this._pendingDrains, { resolve, reject });
    });
  }

  get desiredSize() {
    if (this._closed || this._aborted) return null;
    return this._broadcast._getDesiredSize();
  }

  async write(chunk) {
    return this.writev([chunk]);
  }

  async writev(chunks) {
    if (this._closed || this._aborted) {
      throw new ERR_INVALID_STATE('Writer is closed');
    }

    const converted = ArrayPrototypeMap(chunks, (c) =>
      (typeof c === 'string' ? encoder.encode(c) : c));

    if (this._broadcast._write(converted)) {
      for (let i = 0; i < converted.length; i++) {
        this._totalBytes += converted[i].byteLength;
      }
      return;
    }

    const policy = this._broadcast._options?.backpressure ?? 'strict';
    const highWaterMark = this._broadcast._options?.highWaterMark ?? 16;

    if (policy === 'strict') {
      if (this._pendingWrites.length >= highWaterMark) {
        throw new ERR_INVALID_STATE(
          'Backpressure violation: too many pending writes. ' +
          'Await each write() call to respect backpressure.');
      }
      return new Promise((resolve, reject) => {
        ArrayPrototypePush(this._pendingWrites,
                           { chunk: converted, resolve, reject });
      });
    }

    // 'block' policy
    return new Promise((resolve, reject) => {
      ArrayPrototypePush(this._pendingWrites,
                         { chunk: converted, resolve, reject });
    });
  }

  writeSync(chunk) {
    if (this._closed || this._aborted) return false;
    if (!this._broadcast._canWrite()) return false;
    const converted =
      typeof chunk === 'string' ? encoder.encode(chunk) : chunk;
    if (this._broadcast._write([converted])) {
      this._totalBytes += converted.byteLength;
      return true;
    }
    return false;
  }

  writevSync(chunks) {
    if (this._closed || this._aborted) return false;
    if (!this._broadcast._canWrite()) return false;
    const converted = ArrayPrototypeMap(chunks, (c) =>
      (typeof c === 'string' ? encoder.encode(c) : c));
    if (this._broadcast._write(converted)) {
      for (let i = 0; i < converted.length; i++) {
        this._totalBytes += converted[i].byteLength;
      }
      return true;
    }
    return false;
  }

  async end() {
    if (this._closed) return this._totalBytes;
    this._closed = true;
    this._broadcast._end();
    this._resolvePendingDrains(false);
    return this._totalBytes;
  }

  endSync() {
    if (this._closed) return this._totalBytes;
    this._closed = true;
    this._broadcast._end();
    this._resolvePendingDrains(false);
    return this._totalBytes;
  }

  async abort(reason) {
    if (this._aborted) return;
    this._aborted = true;
    this._closed = true;
    const error = reason ?? new ERR_INVALID_STATE('Aborted');
    this._rejectPendingWrites(error);
    this._rejectPendingDrains(error);
    this._broadcast._abort(error);
  }

  abortSync(reason) {
    if (this._aborted) return true;
    this._aborted = true;
    this._closed = true;
    const error = reason ?? new ERR_INVALID_STATE('Aborted');
    this._rejectPendingWrites(error);
    this._rejectPendingDrains(error);
    this._broadcast._abort(error);
    return true;
  }

  _resolvePendingWrites() {
    while (this._pendingWrites.length > 0 && this._broadcast._canWrite()) {
      const pending = ArrayPrototypeShift(this._pendingWrites);
      if (this._broadcast._write(pending.chunk)) {
        for (let i = 0; i < pending.chunk.length; i++) {
          this._totalBytes += pending.chunk[i].byteLength;
        }
        pending.resolve();
      } else {
        this._pendingWrites.unshift(pending);
        break;
      }
    }
  }

  _rejectPendingWrites(error) {
    const writes = this._pendingWrites;
    this._pendingWrites = [];
    for (let i = 0; i < writes.length; i++) {
      writes[i].reject(error);
    }
  }

  _resolvePendingDrains(canWrite) {
    const drains = this._pendingDrains;
    this._pendingDrains = [];
    for (let i = 0; i < drains.length; i++) {
      drains[i].resolve(canWrite);
    }
  }

  _rejectPendingDrains(error) {
    const drains = this._pendingDrains;
    this._pendingDrains = [];
    for (let i = 0; i < drains.length; i++) {
      drains[i].reject(error);
    }
  }
}

// =============================================================================
// Public API
// =============================================================================

/**
 * Create a broadcast channel for push-model multi-consumer streaming.
 * @param {{ highWaterMark?: number, backpressure?: string, signal?: AbortSignal }} [options]
 * @returns {{ writer: Writer, broadcast: Broadcast }}
 */
function broadcast(options) {
  const opts = {
    highWaterMark: options?.highWaterMark ?? 16,
    backpressure: options?.backpressure ?? 'strict',
    signal: options?.signal,
  };

  const broadcastImpl = new BroadcastImpl(opts);
  const writer = new BroadcastWriter(broadcastImpl);

  if (opts.signal) {
    if (opts.signal.aborted) {
      broadcastImpl.cancel();
    } else {
      opts.signal.addEventListener('abort', () => {
        broadcastImpl.cancel();
      }, { once: true });
    }
  }

  return { writer, broadcast: broadcastImpl };
}

function isBroadcastable(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    broadcastProtocol in value &&
    typeof value[broadcastProtocol] === 'function'
  );
}

const Broadcast = {
  from(input, options) {
    if (isBroadcastable(input)) {
      const bc = input[broadcastProtocol](options);
      return { writer: {}, broadcast: bc };
    }

    const result = broadcast(options);

    (async () => {
      try {
        if (isAsyncIterable(input)) {
          for await (const chunks of input) {
            if (ArrayIsArray(chunks)) {
              await result.writer.writev(chunks);
            }
          }
        } else if (isSyncIterable(input)) {
          for (const chunks of input) {
            if (ArrayIsArray(chunks)) {
              await result.writer.writev(chunks);
            }
          }
        }
        await result.writer.end();
      } catch (error) {
        await result.writer.abort(
          error instanceof Error ? error : new ERR_INVALID_ARG_TYPE('error', 'Error', String(error)));
      }
    })();

    return result;
  },
};

module.exports = {
  broadcast,
  Broadcast,
};
