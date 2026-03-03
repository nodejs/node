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
  ArrayPrototypeSlice,
  Error,
  MathMax,
  Promise,
  PromiseResolve,
  SafeSet,
  String,
  Symbol,
  SymbolAsyncIterator,
  SymbolDispose,
} = primordials;

const { TextEncoder } = require('internal/encoding');

const { lazyDOMException } = require('internal/util');

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

const {
  allUint8Array,
} = require('internal/streams/new/utils');

const {
  RingBuffer,
} = require('internal/streams/new/ringbuffer');

const encoder = new TextEncoder();

// Cached resolved promise to avoid allocating a new one on every sync fast-path.
const kResolvedPromise = PromiseResolve();

// Non-exported symbols for internal cross-class communication between
// BroadcastImpl and BroadcastWriter. Because these symbols are not exported,
// external code cannot access the internal methods/fields.
const kCancelWriter = Symbol('cancelWriter');
const kWrite = Symbol('write');
const kEnd = Symbol('end');
const kAbort = Symbol('abort');
const kGetDesiredSize = Symbol('getDesiredSize');
const kCanWrite = Symbol('canWrite');
const kOnBufferDrained = Symbol('onBufferDrained');

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
  #buffer = new RingBuffer();
  #bufferStart = 0;
  #consumers = new SafeSet();
  #ended = false;
  #error = null;
  #cancelled = false;
  #options;
  #writer = null;

  constructor(options) {
    this.#options = options;
    this[kOnBufferDrained] = null;
  }

  setWriter(writer) {
    this.#writer = writer;
  }

  get backpressurePolicy() {
    return this.#options.backpressure;
  }

  get highWaterMark() {
    return this.#options.highWaterMark;
  }

  get consumerCount() {
    return this.#consumers.size;
  }

  get bufferSize() {
    return this.#buffer.length;
  }

  push(...args) {
    const { transforms, options } = parsePushArgs(args);
    const rawConsumer = this.#createRawConsumer();

    if (transforms.length > 0) {
      if (options?.signal) {
        return pullWithTransforms(
          rawConsumer, ...transforms, { signal: options.signal });
      }
      return pullWithTransforms(rawConsumer, ...transforms);
    }
    return rawConsumer;
  }

  #createRawConsumer() {
    const state = {
      cursor: this.#bufferStart + this.#buffer.length,
      resolve: null,
      reject: null,
      detached: false,
    };

    this.#consumers.add(state);
    const self = this;

    return {
      [SymbolAsyncIterator]() {
        return {
          async next() {
            if (state.detached) {
              // If detached due to an error, throw the error
              if (self.#error) throw self.#error;
              return { __proto__: null, done: true, value: undefined };
            }

            const bufferIndex = state.cursor - self.#bufferStart;
            if (bufferIndex < self.#buffer.length) {
              const chunk = self.#buffer.get(bufferIndex);
              state.cursor++;
              self.#tryTrimBuffer();
              return { __proto__: null, done: false, value: chunk };
            }

            if (self.#error) {
              state.detached = true;
              self.#consumers.delete(state);
              throw self.#error;
            }

            if (self.#ended || self.#cancelled) {
              state.detached = true;
              self.#consumers.delete(state);
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
            self.#consumers.delete(state);
            self.#tryTrimBuffer();
            return { __proto__: null, done: true, value: undefined };
          },

          async throw() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            self.#consumers.delete(state);
            self.#tryTrimBuffer();
            return { __proto__: null, done: true, value: undefined };
          },
        };
      },
    };
  }

  cancel(reason) {
    if (this.#cancelled) return;
    this.#cancelled = true;
    this.#ended = true; // Prevents [kAbort]() from redundantly iterating consumers

    if (reason) {
      this.#error = reason;
    }

    // Reject pending writes on the writer so the pump doesn't hang
    this.#writer?.[kCancelWriter]();

    for (const consumer of this.#consumers) {
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
    this.#consumers.clear();
  }

  [SymbolDispose]() {
    this.cancel();
  }

  // Methods accessed by BroadcastWriter via symbol keys

  [kWrite](chunk) {
    if (this.#ended || this.#cancelled) return false;

    if (this.#buffer.length >= this.#options.highWaterMark) {
      switch (this.#options.backpressure) {
        case 'strict':
        case 'block':
          return false;
        case 'drop-oldest':
          this.#buffer.shift();
          this.#bufferStart++;
          for (const consumer of this.#consumers) {
            if (consumer.cursor < this.#bufferStart) {
              consumer.cursor = this.#bufferStart;
            }
          }
          break;
        case 'drop-newest':
          return true;
      }
    }

    this.#buffer.push(chunk);
    this.#notifyConsumers();
    return true;
  }

  [kEnd]() {
    if (this.#ended) return;
    this.#ended = true;

    for (const consumer of this.#consumers) {
      if (consumer.resolve) {
        const bufferIndex = consumer.cursor - this.#bufferStart;
        if (bufferIndex < this.#buffer.length) {
          const chunk = this.#buffer.get(bufferIndex);
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

  [kAbort](reason) {
    if (this.#ended || this.#error) return;
    this.#error = reason;
    this.#ended = true;

    // Notify all waiting consumers and detach them
    for (const consumer of this.#consumers) {
      if (consumer.reject) {
        consumer.reject(reason);
        consumer.resolve = null;
        consumer.reject = null;
      }
      consumer.detached = true;
    }
    this.#consumers.clear();
  }

  [kGetDesiredSize]() {
    if (this.#ended || this.#cancelled) return null;
    return MathMax(0, this.#options.highWaterMark - this.#buffer.length);
  }

  [kCanWrite]() {
    if (this.#ended || this.#cancelled) return false;
    if ((this.#options.backpressure === 'strict' ||
         this.#options.backpressure === 'block') &&
        this.#buffer.length >= this.#options.highWaterMark) {
      return false;
    }
    return true;
  }

  // Private methods

  #getMinCursor() {
    let min = Infinity;
    for (const consumer of this.#consumers) {
      if (consumer.cursor < min) {
        min = consumer.cursor;
      }
    }
    return min === Infinity ?
      this.#bufferStart + this.#buffer.length : min;
  }

  #tryTrimBuffer() {
    const minCursor = this.#getMinCursor();
    const trimCount = minCursor - this.#bufferStart;
    if (trimCount > 0) {
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = minCursor;

      if (this[kOnBufferDrained] &&
          this.#buffer.length < this.#options.highWaterMark) {
        this[kOnBufferDrained]();
      }
    }
  }

  #notifyConsumers() {
    for (const consumer of this.#consumers) {
      if (consumer.resolve) {
        const bufferIndex = consumer.cursor - this.#bufferStart;
        if (bufferIndex < this.#buffer.length) {
          const chunk = this.#buffer.get(bufferIndex);
          consumer.cursor++;
          const resolve = consumer.resolve;
          consumer.resolve = null;
          consumer.reject = null;
          resolve({ done: false, value: chunk });
        }
      }
    }
  }
}

// =============================================================================
// BroadcastWriter
// =============================================================================

class BroadcastWriter {
  #broadcast;
  #totalBytes = 0;
  #closed = false;
  #aborted = false;
  #pendingWrites = new RingBuffer();
  #pendingDrains = [];

  constructor(broadcastImpl) {
    this.#broadcast = broadcastImpl;

    this.#broadcast[kOnBufferDrained] = () => {
      this.#resolvePendingWrites();
      this.#resolvePendingDrains(true);
    };
  }

  [drainableProtocol]() {
    const desired = this.desiredSize;
    if (desired === null) return null;
    if (desired > 0) return PromiseResolve(true);
    return new Promise((resolve, reject) => {
      ArrayPrototypePush(this.#pendingDrains, { resolve, reject });
    });
  }

  get desiredSize() {
    if (this.#closed || this.#aborted) return null;
    return this.#broadcast[kGetDesiredSize]();
  }

  write(chunk, options) {
    // Fast path: no signal, writer open, buffer has space
    if (!options?.signal && !this.#closed && !this.#aborted &&
        this.#broadcast[kCanWrite]()) {
      const converted =
        typeof chunk === 'string' ? encoder.encode(chunk) : chunk;
      this.#broadcast[kWrite]([converted]);
      this.#totalBytes += converted.byteLength;
      return kResolvedPromise;
    }
    return this.writev([chunk], options);
  }

  writev(chunks, options) {
    // Fast path: no signal, writer open, buffer has space
    if (!options?.signal && !this.#closed && !this.#aborted &&
        this.#broadcast[kCanWrite]()) {
      const converted = allUint8Array(chunks) ?
        ArrayPrototypeSlice(chunks) :
        ArrayPrototypeMap(chunks, (c) =>
          (typeof c === 'string' ? encoder.encode(c) : c));
      this.#broadcast[kWrite](converted);
      for (let i = 0; i < converted.length; i++) {
        this.#totalBytes += converted[i].byteLength;
      }
      return kResolvedPromise;
    }
    return this.#writevSlow(chunks, options);
  }

  async #writevSlow(chunks, options) {
    const signal = options?.signal;

    // Check for pre-aborted signal
    if (signal?.aborted) {
      throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
    }

    if (this.#closed || this.#aborted) {
      throw new ERR_INVALID_STATE('Writer is closed');
    }

    const converted = allUint8Array(chunks) ?
      ArrayPrototypeSlice(chunks) :
      ArrayPrototypeMap(chunks, (c) =>
        (typeof c === 'string' ? encoder.encode(c) : c));

    if (this.#broadcast[kWrite](converted)) {
      for (let i = 0; i < converted.length; i++) {
        this.#totalBytes += converted[i].byteLength;
      }
      return;
    }

    const policy = this.#broadcast.backpressurePolicy;
    const hwm = this.#broadcast.highWaterMark;

    if (policy === 'strict') {
      if (this.#pendingWrites.length >= hwm) {
        throw new ERR_INVALID_STATE(
          'Backpressure violation: too many pending writes. ' +
          'Await each write() call to respect backpressure.');
      }
      return this.#createPendingWrite(converted, signal);
    }

    // 'block' policy
    return this.#createPendingWrite(converted, signal);
  }

  writeSync(chunk) {
    if (this.#closed || this.#aborted) return false;
    if (!this.#broadcast[kCanWrite]()) return false;
    const converted =
      typeof chunk === 'string' ? encoder.encode(chunk) : chunk;
    if (this.#broadcast[kWrite]([converted])) {
      this.#totalBytes += converted.byteLength;
      return true;
    }
    return false;
  }

  writevSync(chunks) {
    if (this.#closed || this.#aborted) return false;
    if (!this.#broadcast[kCanWrite]()) return false;
    const converted = allUint8Array(chunks) ?
      ArrayPrototypeSlice(chunks) :
      ArrayPrototypeMap(chunks, (c) =>
        (typeof c === 'string' ? encoder.encode(c) : c));
    if (this.#broadcast[kWrite](converted)) {
      for (let i = 0; i < converted.length; i++) {
        this.#totalBytes += converted[i].byteLength;
      }
      return true;
    }
    return false;
  }

  // end() is synchronous internally - signal accepted for interface compliance.
  end(options) {
    if (this.#closed) return PromiseResolve(this.#totalBytes);
    this.#closed = true;
    this.#broadcast[kEnd]();
    this.#resolvePendingDrains(false);
    return PromiseResolve(this.#totalBytes);
  }

  endSync() {
    if (this.#closed) return this.#totalBytes;
    this.#closed = true;
    this.#broadcast[kEnd]();
    this.#resolvePendingDrains(false);
    return this.#totalBytes;
  }

  fail(reason) {
    if (this.#aborted) return kResolvedPromise;
    this.#aborted = true;
    this.#closed = true;
    const error = reason ?? new ERR_INVALID_STATE('Failed');
    this.#rejectPendingWrites(error);
    this.#rejectPendingDrains(error);
    this.#broadcast[kAbort](error);
    return kResolvedPromise;
  }

  failSync(reason) {
    if (this.#aborted) return true;
    this.#aborted = true;
    this.#closed = true;
    const error = reason ?? new ERR_INVALID_STATE('Failed');
    this.#rejectPendingWrites(error);
    this.#rejectPendingDrains(error);
    this.#broadcast[kAbort](error);
    return true;
  }

  [kCancelWriter]() {
    if (this.#closed) return;
    this.#closed = true;
    this.#rejectPendingWrites(
      lazyDOMException('Broadcast cancelled', 'AbortError'));
    this.#resolvePendingDrains(false);
  }

  /**
   * Create a pending write promise, optionally racing against a signal.
   * If the signal fires, the entry is removed from pendingWrites and the
   * promise rejects. Signal listeners are cleaned up on normal resolution.
   */
  #createPendingWrite(chunk, signal) {
    return new Promise((resolve, reject) => {
      const entry = { chunk, resolve, reject };
      this.#pendingWrites.push(entry);

      if (!signal) return;

      const onAbort = () => {
        // Remove from queue so it doesn't occupy a slot
        const idx = this.#pendingWrites.indexOf(entry);
        if (idx !== -1) this.#pendingWrites.removeAt(idx);
        reject(signal.reason ?? lazyDOMException('Aborted', 'AbortError'));
      };

      // Wrap resolve/reject to clean up signal listener
      const origResolve = entry.resolve;
      const origReject = entry.reject;
      entry.resolve = function() {
        signal.removeEventListener('abort', onAbort);
        origResolve();
      };
      entry.reject = function(reason) {
        signal.removeEventListener('abort', onAbort);
        origReject(reason);
      };

      signal.addEventListener('abort', onAbort, { once: true });
    });
  }

  #resolvePendingWrites() {
    while (this.#pendingWrites.length > 0 &&
           this.#broadcast[kCanWrite]()) {
      const pending = this.#pendingWrites.shift();
      if (this.#broadcast[kWrite](pending.chunk)) {
        for (let i = 0; i < pending.chunk.length; i++) {
          this.#totalBytes += pending.chunk[i].byteLength;
        }
        pending.resolve();
      } else {
        this.#pendingWrites.unshift(pending);
        break;
      }
    }
  }

  #rejectPendingWrites(error) {
    while (this.#pendingWrites.length > 0) {
      this.#pendingWrites.shift().reject(error);
    }
  }

  #resolvePendingDrains(canWrite) {
    const drains = this.#pendingDrains;
    this.#pendingDrains = [];
    for (let i = 0; i < drains.length; i++) {
      drains[i].resolve(canWrite);
    }
  }

  #rejectPendingDrains(error) {
    const drains = this.#pendingDrains;
    this.#pendingDrains = [];
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
    highWaterMark: MathMax(1, options?.highWaterMark ?? 16),
    backpressure: options?.backpressure ?? 'strict',
    signal: options?.signal,
  };

  const broadcastImpl = new BroadcastImpl(opts);
  const writer = new BroadcastWriter(broadcastImpl);
  broadcastImpl.setWriter(writer);

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
    const signal = options?.signal;

    (async () => {
      try {
        if (isAsyncIterable(input)) {
          for await (const chunks of input) {
            if (signal?.aborted) {
              throw signal.reason ??
                lazyDOMException('Aborted', 'AbortError');
            }
            if (ArrayIsArray(chunks)) {
              await result.writer.writev(
                chunks, signal ? { signal } : undefined);
            }
          }
        } else if (isSyncIterable(input)) {
          for (const chunks of input) {
            if (signal?.aborted) {
              throw signal.reason ??
                lazyDOMException('Aborted', 'AbortError');
            }
            if (ArrayIsArray(chunks)) {
              await result.writer.writev(
                chunks, signal ? { signal } : undefined);
            }
          }
        }
        await result.writer.end(signal ? { signal } : undefined);
      } catch (error) {
        await result.writer.fail(
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
