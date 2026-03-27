'use strict';

// New Streams API - Broadcast
//
// Push-model multi-consumer streaming. A single writer can push data to
// multiple consumers. Each consumer has an independent cursor into a
// shared buffer.

const {
  ArrayIsArray,
  ArrayPrototypePush,
  MathMax,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SafeSet,
  Symbol,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const { lazyDOMException } = require('internal/util');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_RETURN_VALUE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const {
  validateAbortSignal,
  validateInteger,
  validateObject,
} = require('internal/validators');

const {
  broadcastProtocol,
  drainableProtocol,
} = require('internal/streams/iter/types');

const {
  isAsyncIterable,
  isSyncIterable,
} = require('internal/streams/iter/from');

const {
  pull: pullWithTransforms,
} = require('internal/streams/iter/pull');

const {
  kMultiConsumerDefaultHWM,
  kResolvedPromise,
  clampHWM,
  convertChunks,
  getMinCursor,
  hasProtocol,
  onSignalAbort,
  parsePullArgs,
  wrapError,
  toUint8Array,
  validateBackpressure,
} = require('internal/streams/iter/utils');

const {
  RingBuffer,
} = require('internal/streams/iter/ringbuffer');

const kCancelWriter = Symbol('kCancelWriter');
const kWrite = Symbol('kWrite');
const kEnd = Symbol('kEnd');
const kAbort = Symbol('kAbort');
const kGetDesiredSize = Symbol('kGetDesiredSize');
const kCanWrite = Symbol('kCanWrite');
const kOnBufferDrained = Symbol('kOnBufferDrained');

// =============================================================================
// Broadcast Implementation
// =============================================================================

class BroadcastImpl {
  #buffer = new RingBuffer();
  #bufferStart = 0;
  #consumers = new SafeSet();
  #waiters = [];  // Consumers with pending resolve (subset of #consumers)
  #ended = false;
  #error = null;
  #cancelled = false;
  #options;
  #writer = null;
  #cachedMinCursor = 0;
  #minCursorDirty = false;

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
    const { transforms, options } = parsePullArgs(args);
    const rawConsumer = this.#createRawConsumer();

    // When transforms are present, delegate to pull() which creates its
    // own internal AbortController that follows the external signal.
    // When no transforms, return rawConsumer directly (controller elided
    // per PULL-02 optimization -- no transforms means no signal recipient).
    if (transforms.length > 0) {
      const pullArgs = [...transforms];
      if (options?.signal) {
        ArrayPrototypePush(pullArgs,
                           { __proto__: null, signal: options.signal });
      }
      return pullWithTransforms(rawConsumer, ...pullArgs);
    }
    return rawConsumer;
  }

  #createRawConsumer() {
    const state = {
      __proto__: null,
      // Start at the oldest buffered entry so late-joining consumers
      // can read data already in the buffer.
      cursor: this.#bufferStart,
      resolve: null,
      reject: null,
      detached: false,
    };

    this.#consumers.add(state);
    // New consumer starts at buffer start; recalculate min cursor
    // since this consumer may now be the slowest.
    if (this.#consumers.size === 1) {
      this.#cachedMinCursor = state.cursor;
      this.#minCursorDirty = false;
    } else {
      this.#minCursorDirty = true;
    }
    const self = this;

    const kDone = PromiseResolve(
      { __proto__: null, done: true, value: undefined });

    function detach() {
      state.detached = true;
      state.resolve = null;
      state.reject = null;
      self.#consumers.delete(state);
      self.#minCursorDirty = true;
      self.#tryTrimBuffer();
    }

    return {
      __proto__: null,
      [SymbolAsyncIterator]() {
        return {
          __proto__: null,
          next() {
            if (state.detached) {
              if (self.#error) return PromiseReject(self.#error);
              return kDone;
            }

            const bufferIndex = state.cursor - self.#bufferStart;
            if (bufferIndex < self.#buffer.length) {
              const chunk = self.#buffer.get(bufferIndex);
              // If this consumer was at the min cursor, mark dirty
              if (state.cursor <= self.#cachedMinCursor) {
                self.#minCursorDirty = true;
              }
              state.cursor++;
              self.#tryTrimBuffer();
              return PromiseResolve(
                { __proto__: null, done: false, value: chunk });
            }

            if (self.#error) {
              state.detached = true;
              self.#consumers.delete(state);
              return PromiseReject(self.#error);
            }

            if (self.#ended || self.#cancelled) {
              detach();
              return kDone;
            }

            const { promise, resolve, reject } = PromiseWithResolvers();
            state.resolve = resolve;
            state.reject = reject;
            ArrayPrototypePush(self.#waiters, state);
            return promise;
          },

          return() {
            detach();
            return kDone;
          },

          throw() {
            detach();
            return kDone;
          },
        };
      },
    };
  }

  cancel(reason) {
    if (this.#cancelled) return;
    this.#cancelled = true;
    this.#ended = true; // Prevents [kAbort]() from redundantly iterating consumers

    if (reason !== undefined) {
      this.#error = reason;
    }

    // Reject pending writes on the writer so the pump doesn't hang
    this.#writer?.[kCancelWriter]();

    for (const consumer of this.#consumers) {
      if (consumer.resolve) {
        if (reason !== undefined) {
          consumer.reject?.(reason);
        } else {
          consumer.resolve({ __proto__: null, done: true, value: undefined });
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
          consumer.resolve({ __proto__: null, done: false, value: chunk });
        } else {
          consumer.resolve({ __proto__: null, done: true, value: undefined });
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

  #recomputeMinCursor() {
    this.#cachedMinCursor = getMinCursor(
      this.#consumers, this.#bufferStart + this.#buffer.length);
    this.#minCursorDirty = false;
  }

  #tryTrimBuffer() {
    if (this.#minCursorDirty) {
      this.#recomputeMinCursor();
    }
    const trimCount = this.#cachedMinCursor - this.#bufferStart;
    if (trimCount > 0) {
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = this.#cachedMinCursor;

      if (this[kOnBufferDrained] &&
          this.#buffer.length < this.#options.highWaterMark) {
        this[kOnBufferDrained]();
      }
    }
  }

  #notifyConsumers() {
    const waiters = this.#waiters;
    if (waiters.length === 0) return;
    // Swap out the waiters list so consumers that re-wait during
    // resolve don't get processed twice in this cycle.
    this.#waiters = [];
    for (let i = 0; i < waiters.length; i++) {
      const consumer = waiters[i];
      if (consumer.resolve) {
        const bufferIndex = consumer.cursor - this.#bufferStart;
        if (bufferIndex < this.#buffer.length) {
          const chunk = this.#buffer.get(bufferIndex);
          if (consumer.cursor <= this.#cachedMinCursor) {
            this.#minCursorDirty = true;
          }
          consumer.cursor++;
          const resolve = consumer.resolve;
          consumer.resolve = null;
          consumer.reject = null;
          resolve({ __proto__: null, done: false, value: chunk });
        } else {
          // Still waiting -- put back
          ArrayPrototypePush(this.#waiters, consumer);
        }
      }
    }
  }
}

// =============================================================================
// BroadcastWriter
// =============================================================================

let getBroadcastPendingWrites;

class BroadcastWriter {
  #broadcast;
  #totalBytes = 0;
  #closed;
  #aborted = false;
  #pendingWrites = new RingBuffer();
  #pendingDrains = [];

  static {
    // Used in wireBroadcastWriteSignal ensure the signal listener can be
    // constructed without closing over the chunk data, which may be large.
    getBroadcastPendingWrites = (obj) => obj.#pendingWrites;
  }

  constructor(broadcastImpl) {
    this.#broadcast = broadcastImpl;

    this.#broadcast[kOnBufferDrained] = () => {
      this.#resolvePendingWrites();
      this.#resolvePendingDrains(true);
    };
  }

  // The drainable protocol works with Stream.ondrain to provide a notification
  // when the writer can accept more data after being backpressured.
  [drainableProtocol]() {
    const desired = this.desiredSize;
    if (desired === null) return null;
    if (desired > 0) return PromiseResolve(true);
    const { promise, resolve, reject } = PromiseWithResolvers();
    ArrayPrototypePush(this.#pendingDrains, { __proto__: null, resolve, reject });
    return promise;
  }

  #isClosed() {
    return this.#closed !== undefined;
  }

  #isClosedOrAborted() {
    return this.#isClosed() || this.#aborted;
  }

  get desiredSize() {
    return this.#isClosedOrAborted() ? null : this.#broadcast[kGetDesiredSize]();
  }

  #canUseWriteFastPath(options) {
    return !options?.signal && !this.#isClosed() && !this.#aborted &&
        this.#broadcast[kCanWrite]();
  }

  write(chunk, options) {
    // Fast path: no signal, writer open, buffer has space
    if (this.#canUseWriteFastPath(options)) {
      const converted = toUint8Array(chunk);
      this.#broadcast[kWrite]([converted]);
      this.#totalBytes += TypedArrayPrototypeGetByteLength(converted);
      return kResolvedPromise;
    }
    return this.#writevSlow([chunk], options);
  }

  writev(chunks, options) {
    // Fast path: no signal, writer open, buffer has space
    if (this.#canUseWriteFastPath(options)) {
      const converted = convertChunks(chunks);
      this.#broadcast[kWrite](converted);
      for (let i = 0; i < converted.length; i++) {
        this.#totalBytes += TypedArrayPrototypeGetByteLength(converted[i]);
      }
      return kResolvedPromise;
    }
    return this.#writevSlow(chunks, options);
  }

  async #writevSlow(chunks, options) {
    const signal = options?.signal;

    // Check for pre-aborted
    signal?.throwIfAborted();

    if (this.#isClosedOrAborted()) {
      throw new ERR_INVALID_STATE.TypeError('Writer is closed');
    }

    const converted = convertChunks(chunks);

    if (this.#broadcast[kWrite](converted)) {
      for (let i = 0; i < converted.length; i++) {
        this.#totalBytes += TypedArrayPrototypeGetByteLength(converted[i]);
      }
      return;
    }

    const policy = this.#broadcast.backpressurePolicy;
    const hwm = this.#broadcast.highWaterMark;

    if (policy === 'strict') {
      if (this.#pendingWrites.length >= hwm) {
        throw new ERR_INVALID_STATE.TypeError(
          'Backpressure violation: too many pending writes. ' +
          'Await each write() call to respect backpressure.');
      }
      return this.#createPendingWrite(converted, signal);
    }

    // 'block' policy
    return this.#createPendingWrite(converted, signal);
  }

  writeSync(chunk) {
    if (this.#isClosedOrAborted()) return false;
    if (!this.#broadcast[kCanWrite]()) return false;
    const converted =
      toUint8Array(chunk);
    if (this.#broadcast[kWrite]([converted])) {
      this.#totalBytes += TypedArrayPrototypeGetByteLength(converted);
      return true;
    }
    return false;
  }

  writevSync(chunks) {
    if (this.#isClosedOrAborted()) return false;
    if (!this.#broadcast[kCanWrite]()) return false;
    const converted = convertChunks(chunks);
    if (this.#broadcast[kWrite](converted)) {
      for (let i = 0; i < converted.length; i++) {
        this.#totalBytes += TypedArrayPrototypeGetByteLength(converted[i]);
      }
      return true;
    }
    return false;
  }

  // end() is synchronous internally - signal accepted for interface compliance.
  end(options) {
    if (this.#isClosed()) return this.#closed;
    this.#closed = PromiseResolve(this.#totalBytes);
    this.#broadcast[kEnd]();
    this.#resolvePendingDrains(false);
    return this.#closed;
  }

  endSync() {
    if (this.#closed) return this.#totalBytes;
    this.#closed = PromiseResolve(this.#totalBytes);
    this.#broadcast[kEnd]();
    this.#resolvePendingDrains(false);
    return this.#totalBytes;
  }

  fail(reason) {
    if (this.#isClosedOrAborted()) return;
    this.#aborted = true;
    this.#closed = PromiseResolve(this.#totalBytes);
    const error = reason ?? new ERR_INVALID_STATE.TypeError('Failed');
    this.#rejectPendingWrites(error);
    this.#rejectPendingDrains(error);
    this.#broadcast[kAbort](error);
  }

  [SymbolAsyncDispose]() {
    this.fail();
    return PromiseResolve();
  }

  [SymbolDispose]() {
    this.fail();
  }

  [kCancelWriter]() {
    if (this.#isClosed()) return;
    this.#closed = PromiseResolve(this.#totalBytes);
    this.#rejectPendingWrites(
      lazyDOMException('Broadcast cancelled', 'AbortError'));
    this.#resolvePendingDrains(false);
  }

  /**
   * Create a pending write promise, optionally racing against a signal.
   * If the signal fires, the entry is removed from pendingWrites and the
   * promise rejects. Signal listeners are cleaned up on normal resolution.
   * @returns {Promise<void>}
   */
  #createPendingWrite(chunk, signal) {
    const { promise, resolve, reject } = PromiseWithResolvers();
    const entry = { __proto__: null, chunk, resolve, reject };
    this.#pendingWrites.push(entry);
    if (signal) {
      wireBroadcastWriteSignal(entry, signal, resolve, reject, this);
    }
    return promise;
  }

  #resolvePendingWrites() {
    while (this.#pendingWrites.length > 0 && this.#broadcast[kCanWrite]()) {
      const pending = this.#pendingWrites.shift();
      if (this.#broadcast[kWrite](pending.chunk)) {
        for (let i = 0; i < pending.chunk.length; i++) {
          this.#totalBytes += TypedArrayPrototypeGetByteLength(pending.chunk[i]);
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

function wireBroadcastWriteSignal(entry, signal, resolve, reject, self) {
  const onAbort = () => {
    const pendingWrites = getBroadcastPendingWrites(self);
    const idx = pendingWrites.indexOf(entry);
    if (idx !== -1) pendingWrites.removeAt(idx);
    entry.chunk = null;
    reject(signal.reason ?? lazyDOMException('Aborted', 'AbortError'));
  };
  entry.resolve = function() {
    signal.removeEventListener('abort', onAbort);
    entry.chunk = null;
    resolve();
  };
  entry.reject = function(reason) {
    signal.removeEventListener('abort', onAbort);
    entry.chunk = null;
    reject(reason);
  };
  signal.addEventListener('abort', onAbort, { __proto__: null, once: true });
}

// =============================================================================
// Public API
// =============================================================================

/**
 * Create a broadcast channel for push-model multi-consumer streaming.
 * @param {{ highWaterMark?: number, backpressure?: string, signal?: AbortSignal }} [options]
 * @returns {{ writer: Writer, broadcast: Broadcast }}
 */
function broadcast(options = { __proto__: null }) {
  validateObject(options, 'options');
  const {
    highWaterMark = kMultiConsumerDefaultHWM,
    backpressure = 'strict',
    signal,
  } = options;
  validateInteger(highWaterMark, 'options.highWaterMark');
  validateBackpressure(backpressure);
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
  }

  const opts = {
    __proto__: null,
    highWaterMark: clampHWM(highWaterMark),
    backpressure,
    signal,
  };

  const broadcastImpl = new BroadcastImpl(opts);
  const writer = new BroadcastWriter(broadcastImpl);
  broadcastImpl.setWriter(writer);

  if (signal) {
    onSignalAbort(signal, () => broadcastImpl.cancel());
  }

  return { __proto__: null, writer, broadcast: broadcastImpl };
}

function isBroadcastable(value) {
  return hasProtocol(value, broadcastProtocol);
}

const Broadcast = {
  __proto__: null,
  from(input, options) {
    if (isBroadcastable(input)) {
      const bc = input[broadcastProtocol](options);
      if (bc === null || typeof bc !== 'object') {
        throw new ERR_INVALID_RETURN_VALUE(
          'an object', '[Symbol.for(\'Stream.broadcastProtocol\')]', bc);
      }
      return { __proto__: null, writer: { __proto__: null }, broadcast: bc };
    }

    if (!isAsyncIterable(input) && !isSyncIterable(input)) {
      throw new ERR_INVALID_ARG_TYPE(
        'input', ['Broadcastable', 'AsyncIterable', 'Iterable'], input);
    }

    const result = broadcast(options);
    const signal = options?.signal;

    const pump = async () => {
      const w = result.writer;
      try {
        if (isAsyncIterable(input)) {
          for await (const chunks of input) {
            signal?.throwIfAborted();
            if (ArrayIsArray(chunks)) {
              if (!w.writevSync(chunks)) {
                await w.writev(chunks, signal ? { signal } : undefined);
              }
            } else if (!w.writeSync(chunks)) {
              await w.write(chunks, signal ? { signal } : undefined);
            }
          }
        } else if (isSyncIterable(input)) {
          for (const chunks of input) {
            signal?.throwIfAborted();
            if (ArrayIsArray(chunks)) {
              if (!w.writevSync(chunks)) {
                await w.writev(chunks, signal ? { signal } : undefined);
              }
            } else if (!w.writeSync(chunks)) {
              await w.write(chunks, signal ? { signal } : undefined);
            }
          }
        }
        if (w.endSync() < 0) {
          await w.end(signal ? { signal } : undefined);
        }
      } catch (error) {
        w.fail(wrapError(error));
      }
    };
    PromisePrototypeThen(pump(), undefined, () => {});

    return result;
  },
};

module.exports = {
  Broadcast,
  broadcast,
};
