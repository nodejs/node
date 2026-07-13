'use strict';

// New Streams API - Broadcast
//
// Push-model multi-consumer streaming. A single writer can push data to
// multiple consumers. Each consumer has an independent cursor into a
// shared buffer.

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ArrayPrototypeShift,
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
  from,
  isAsyncIterable,
  isSyncIterable,
} = require('internal/streams/iter/from');

const {
  pull: pullWithTransforms,
} = require('internal/streams/iter/pull');

const {
  kMultiConsumerDefaultBudget,
  kResolvedPromise,
  convertChunks,
  getWriterSignal,
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
  #cachedMinCursorConsumers = 0;
  /** Cumulative byte size of buffered entries */
  #bufferedBytes = 0;

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

  get consumerCount() {
    return this.#consumers.size;
  }

  push(...args) {
    const { transforms, options } = parsePullArgs(args);
    const rawConsumer = this.#createRawConsumer();

    // When transforms are present, delegate to pull() which creates its
    // own internal AbortController that follows the external signal.
    // When no transforms, return rawConsumer directly (controller elided
    // per PULL-02 optimization -- no transforms means no signal recipient).
    if (transforms.length > 0 || options?.signal) {
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
      pending: [],
      detached: false,
    };

    this.#consumers.add(state);
    if (this.#consumers.size === 1) {
      this.#cachedMinCursor = state.cursor;
      this.#cachedMinCursorConsumers = 1;
    } else if (state.cursor === this.#cachedMinCursor) {
      this.#cachedMinCursorConsumers++;
    } else {
      this.#recomputeMinCursor();
    }
    const self = this;

    const kDone = PromiseResolve(
      { __proto__: null, done: true, value: undefined });

    function detach() {
      state.detached = true;
      if (state.resolve) {
        state.resolve({ __proto__: null, done: true, value: undefined });
      }
      self.#resolvePendingDone(state);
      if (self.#deleteConsumer(state)) {
        self.#tryTrimBuffer();
      }
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
              const cursor = state.cursor;
              state.cursor++;
              if (cursor === self.#cachedMinCursor &&
                  --self.#cachedMinCursorConsumers === 0) {
                self.#tryTrimBuffer();
              }
              return PromiseResolve(
                { __proto__: null, done: false, value: chunk });
            }

            if (self.#error) {
              state.detached = true;
              self.#deleteConsumer(state);
              return PromiseReject(self.#error);
            }

            if (self.#ended || self.#cancelled) {
              detach();
              return kDone;
            }

            if (state.resolve) {
              const { promise, resolve, reject } = PromiseWithResolvers();
              ArrayPrototypePush(state.pending,
                                 { __proto__: null, resolve, reject });
              return promise;
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
      if (reason !== undefined) {
        this.#rejectPending(consumer, reason);
      } else {
        this.#resolvePendingDone(consumer);
      }
      consumer.detached = true;
    }
    this.#consumers.clear();
    this.#cachedMinCursorConsumers = 0;
  }

  [SymbolDispose]() {
    this.cancel();
  }

  // Methods accessed by BroadcastWriter via symbol keys

  [kWrite](chunk) {
    if (this.#ended || this.#cancelled) return false;

    if (this.#bufferedBytes >= this.#options.budget) {
      switch (this.#options.backpressure) {
        case 'strict':
        case 'unbounded':
          return false;
        case 'drop-oldest':
          while (this.#bufferedBytes >= this.#options.budget &&
                 this.#buffer.length > 0) {
            const evicted = this.#buffer.shift();
            this.#bufferedBytes -= this.#batchByteSize(evicted);
            this.#bufferStart++;
          }
          for (const consumer of this.#consumers) {
            if (consumer.cursor < this.#bufferStart) {
              this.#deleteConsumerFromMin(consumer);
              consumer.cursor = this.#bufferStart;
            }
          }
          this.#recomputeMinCursor();
          break;
        case 'drop-newest':
          return true;
      }
    }

    const batchSize = this.#batchByteSize(chunk);
    this.#buffer.push(chunk);
    this.#bufferedBytes += batchSize;
    this.#notifyConsumers();
    return true;
  }

  [kEnd]() {
    if (this.#ended) return;
    this.#ended = true;

    for (const consumer of this.#consumers) {
      while (consumer.resolve) {
        const bufferIndex = consumer.cursor - this.#bufferStart;
        if (bufferIndex < this.#buffer.length) {
          const chunk = this.#buffer.get(bufferIndex);
          const cursor = consumer.cursor;
          consumer.cursor++;
          if (cursor === this.#cachedMinCursor &&
              --this.#cachedMinCursorConsumers === 0) {
            this.#tryTrimBuffer();
          }
          consumer.resolve({ __proto__: null, done: false, value: chunk });
        } else {
          consumer.resolve({ __proto__: null, done: true, value: undefined });
          this.#resolvePendingDone(consumer);
          consumer.detached = true;
        }
        consumer.resolve = null;
        consumer.reject = null;
        if (consumer.detached && this.#deleteConsumer(consumer)) {
          this.#tryTrimBuffer();
          break;
        }
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
      this.#rejectPending(consumer, reason);
      consumer.detached = true;
    }
    this.#consumers.clear();
    this.#cachedMinCursorConsumers = 0;
  }

  /**
   * Check if the next write is likely to be accepted.
   * Returns null if ended/cancelled, true/false otherwise.
   * @returns {boolean | null}
   */
  [kCanWrite]() {
    if (this.#ended || this.#cancelled) return null;
    if ((this.#options.backpressure === 'strict' ||
        this.#options.backpressure === 'unbounded') &&
        this.#bufferedBytes >= this.#options.budget) {
      return false;
    }
    return true;
  }

  // Private methods

  #recomputeMinCursor() {
    const { minCursor, minCursorConsumers } = getMinCursor(
      this.#consumers, this.#bufferStart + this.#buffer.length);
    this.#cachedMinCursor = minCursor;
    this.#cachedMinCursorConsumers = minCursorConsumers;
  }

  #tryTrimBuffer() {
    if (this.#cachedMinCursorConsumers === 0) {
      this.#recomputeMinCursor();
    }
    const trimCount = this.#cachedMinCursor - this.#bufferStart;
    if (trimCount > 0) {
      for (let i = 0; i < trimCount; i++) {
        const evicted = this.#buffer.get(i);
        this.#bufferedBytes -= this.#batchByteSize(evicted);
      }
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = this.#cachedMinCursor;

      if (this[kOnBufferDrained] &&
          this.#bufferedBytes < this.#options.budget) {
        this[kOnBufferDrained]();
      }
    }
  }

  #batchByteSize(batch) {
    let size = 0;
    for (let i = 0; i < batch.length; i++) {
      size += TypedArrayPrototypeGetByteLength(batch[i]);
    }
    return size;
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
          const cursor = consumer.cursor;
          consumer.cursor++;
          if (cursor === this.#cachedMinCursor &&
              --this.#cachedMinCursorConsumers === 0) {
            this.#tryTrimBuffer();
          }
          const resolve = consumer.resolve;
          consumer.resolve = null;
          consumer.reject = null;
          resolve({ __proto__: null, done: false, value: chunk });
          if (consumer.detached && this.#deleteConsumer(consumer)) {
            this.#tryTrimBuffer();
          } else if (this.#promotePending(consumer)) {
            ArrayPrototypePush(this.#waiters, consumer);
          }
        } else {
          // Still waiting -- put back
          ArrayPrototypePush(this.#waiters, consumer);
        }
      }
    }
  }

  #deleteConsumerFromMin(consumer) {
    if (consumer.cursor === this.#cachedMinCursor) {
      this.#cachedMinCursorConsumers--;
      return this.#cachedMinCursorConsumers === 0;
    }
    return false;
  }

  #deleteConsumer(consumer) {
    if (this.#consumers.delete(consumer)) {
      return this.#deleteConsumerFromMin(consumer);
    }
    return false;
  }

  #promotePending(consumer) {
    const next = ArrayPrototypeShift(consumer.pending);
    if (next === undefined) return false;
    consumer.resolve = next.resolve;
    consumer.reject = next.reject;
    return true;
  }

  #resolvePendingDone(consumer) {
    if (consumer.resolve) {
      consumer.resolve = null;
      consumer.reject = null;
    }
    while (consumer.pending.length > 0) {
      ArrayPrototypeShift(consumer.pending).resolve(
        { __proto__: null, done: true, value: undefined });
    }
  }

  #rejectPending(consumer, reason) {
    while (consumer.pending.length > 0) {
      ArrayPrototypeShift(consumer.pending).reject(reason);
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
    const canWrite = this.canWrite;
    if (canWrite === null) return null;
    if (canWrite) return PromiseResolve(true);
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

  get canWrite() {
    return this.#isClosedOrAborted() ? null : this.#broadcast[kCanWrite]();
  }

  #canUseWriteFastPath(signal) {
    return !signal && !this.#isClosed() && !this.#aborted &&
        this.#broadcast[kCanWrite]();
  }

  write(chunk, options) {
    const signal = getWriterSignal(options);
    // Fast path: no signal, writer open, buffer has space
    if (this.#canUseWriteFastPath(signal)) {
      const converted = toUint8Array(chunk);
      this.#broadcast[kWrite]([converted]);
      this.#totalBytes += TypedArrayPrototypeGetByteLength(converted);
      return kResolvedPromise;
    }
    return this.#writevSlow([chunk], signal);
  }

  writev(chunks, options) {
    if (!ArrayIsArray(chunks)) {
      throw new ERR_INVALID_ARG_TYPE('chunks', 'Array', chunks);
    }
    const signal = getWriterSignal(options);
    // Fast path: no signal, writer open, buffer has space
    if (this.#canUseWriteFastPath(signal)) {
      const converted = convertChunks(chunks);
      this.#broadcast[kWrite](converted);
      for (let i = 0; i < converted.length; i++) {
        this.#totalBytes += TypedArrayPrototypeGetByteLength(converted[i]);
      }
      return kResolvedPromise;
    }
    return this.#writevSlow(chunks, signal);
  }

  async #writevSlow(chunks, signal) {
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

    if (policy === 'strict') {
      if (this.#pendingWrites.length >= 1) {
        throw new ERR_INVALID_STATE.TypeError(
          'Backpressure violation: too many pending writes. ' +
          'Await each write() call to respect backpressure.');
      }
      return this.#createPendingWrite(converted, signal);
    }

    // 'unbounded' policy
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
    if (!ArrayIsArray(chunks)) {
      throw new ERR_INVALID_ARG_TYPE('chunks', 'Array', chunks);
    }
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
    getWriterSignal(options);
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

function onBroadcastCancel(broadcastImpl, signal) {
  onSignalAbort(signal, () => broadcastImpl.cancel(signal.reason));
}

// =============================================================================
// Public API
// =============================================================================

/**
 * Create a broadcast channel for push-model multi-consumer streaming.
 * @param {{ budget?: number, backpressure?: string, signal?: AbortSignal }} [options]
 * @returns {{ writer: Writer, broadcast: Broadcast }}
 */
function broadcast(options = { __proto__: null }) {
  validateObject(options, 'options');
  const {
    budget = kMultiConsumerDefaultBudget,
    backpressure = 'strict',
    signal,
  } = options;
  validateInteger(budget, 'options.budget', 16384);
  validateBackpressure(backpressure);
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
  }

  const opts = {
    __proto__: null,
    budget,
    backpressure,
    signal,
  };

  const broadcastImpl = new BroadcastImpl(opts);
  const writer = new BroadcastWriter(broadcastImpl);
  broadcastImpl.setWriter(writer);

  if (signal) {
    onBroadcastCancel(broadcastImpl, signal);
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

    const source = from(input);

    if (!isAsyncIterable(source) && !isSyncIterable(source)) {
      throw new ERR_INVALID_ARG_TYPE(
        'input', ['Broadcastable', 'AsyncIterable', 'Iterable'], input);
    }

    const result = broadcast(options);
    const signal = options?.signal;

    const pump = async () => {
      const w = result.writer;
      try {
        if (isAsyncIterable(source)) {
          for await (const chunks of source) {
            signal?.throwIfAborted();
            if (ArrayIsArray(chunks)) {
              if (!w.writevSync(chunks)) {
                await w.writev(chunks, signal ? { signal } : undefined);
              }
            } else if (!w.writeSync(chunks)) {
              await w.write(chunks, signal ? { signal } : undefined);
            }
          }
        } else if (isSyncIterable(source)) {
          for (const chunks of source) {
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
