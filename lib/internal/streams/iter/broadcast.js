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
  SafePromisePrototypeFinally,
  SafePromiseRace,
  SafeSet,
  Symbol,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
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
  validateInteger,
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
  pullWithConsumerCleanup,
} = require('internal/streams/iter/pull');

const {
  kMultiConsumerDefaultBudget,
  kResolvedPromise,
  convertChunks,
  createBatchEntry,
  getWriterSignal,
  getMinCursor,
  hasProtocol,
  onSignalAbort,
  parsePullArgs,
  wrapError,
  toWriterUint8Array,
  validateBatchEntry,
} = require('internal/streams/iter/utils');
const {
  converters,
} = require('internal/streams/iter/webidl');

const {
  RingBuffer,
} = require('internal/streams/iter/ringbuffer');

const kCancelWriter = Symbol('kCancelWriter');
const kWrite = Symbol('kWrite');
const kEnd = Symbol('kEnd');
const kAbort = Symbol('kAbort');
const kCanWrite = Symbol('kCanWrite');
const kOnBufferDrained = Symbol('kOnBufferDrained');
const kOnEndDrained = Symbol('kOnEndDrained');
const kPendingWriteRemoved = Symbol('kPendingWriteRemoved');

function raceEndWithSignal(promise, signal) {
  if (!signal) return promise;

  const { promise: aborted, reject } = PromiseWithResolvers();
  const onAbort = () => reject(signal.reason);
  signal.addEventListener('abort', onAbort, { __proto__: null, once: true });
  if (signal.aborted) onAbort();

  return SafePromisePrototypeFinally(
    SafePromiseRace([promise, aborted]),
    () => signal.removeEventListener('abort', onAbort),
  );
}

// =============================================================================
// Broadcast Implementation
// =============================================================================

class BroadcastImpl {
  #buffer = new RingBuffer();
  #bufferStart = 0;
  #consumers = new SafeSet();
  #waiters = [];  // Consumers with pending resolve (subset of #consumers)
  #ended = false;
  #error;
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
    this[kOnEndDrained] = null;
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
    const parsed = parsePullArgs(args);
    const { transforms } = parsed;
    const options = converters.PullOptions(parsed.options, {
      __proto__: null,
      context: 'options',
    });
    const { signal } = options;

    // Avoid registering a consumer that the pre-aborted pipeline will never
    // read or detach.
    if (signal?.aborted) {
      return {
        __proto__: null,
        // eslint-disable-next-line require-yield
        async *[SymbolAsyncIterator]() {
          throw signal.reason;
        },
      };
    }

    const rawConsumer = this.#createRawConsumer();

    // When transforms are present, delegate to pull() which creates its
    // own internal AbortController that follows the external signal.
    // When no transforms, return rawConsumer directly (controller elided
    // per PULL-02 optimization -- no transforms means no signal recipient).
    if (transforms.length > 0 || signal) {
      return pullWithConsumerCleanup(rawConsumer, transforms, signal);
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
      self.#notifyEndDrained();
    }

    return {
      __proto__: null,
      [SymbolAsyncIterator]() {
        return {
          __proto__: null,
          next() {
            if (state.detached) {
              if (self.#error !== undefined) {
                return PromiseReject(self.#error);
              }
              return kDone;
            }

            const bufferIndex = state.cursor - self.#bufferStart;
            if (bufferIndex < self.#buffer.length) {
              const chunk = self.#readEntry(self.#buffer.get(bufferIndex));
              if (chunk === null) return PromiseReject(self.#error);
              const cursor = state.cursor;
              state.cursor++;
              if (cursor === self.#cachedMinCursor &&
                  --self.#cachedMinCursorConsumers === 0) {
                self.#tryTrimBuffer();
              }
              return PromiseResolve(
                { __proto__: null, done: false, value: chunk });
            }

            if (self.#error !== undefined) {
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

  [kWrite](entry) {
    if (this.#ended || this.#cancelled) return false;

    const batchSize = entry.byteLength;
    let droppedOldest = false;

    // Skip empty chunks -- zero-byte writes would accumulate infinitely
    // without ever triggering backpressure under a byte-budget model.
    if (batchSize === 0) return true;

    if (this.#bufferedBytes >= this.#options.budget) {
      switch (this.#options.backpressure) {
        case 'strict':
        case 'unbounded':
          return false;
        case 'drop-oldest':
          droppedOldest = true;
          while (this.#bufferedBytes >= this.#options.budget &&
                 this.#buffer.length > 0) {
            const evicted = this.#buffer.shift();
            this.#bufferedBytes -= evicted.byteLength;
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

    this.#buffer.push(entry);
    this.#bufferedBytes += batchSize;
    this.#notifyConsumers();
    if (droppedOldest &&
        this.#bufferedBytes < this.#options.budget) {
      this[kOnBufferDrained]?.();
    }
    return true;
  }

  [kEnd]() {
    if (this.#ended) return;
    this.#ended = true;

    for (const consumer of this.#consumers) {
      while (consumer.resolve) {
        const bufferIndex = consumer.cursor - this.#bufferStart;
        if (bufferIndex < this.#buffer.length) {
          const chunk = this.#readEntry(this.#buffer.get(bufferIndex));
          if (chunk === null) return;
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
    this.#notifyEndDrained();
  }

  [kAbort](reason) {
    if (this.#error !== undefined) return;
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
   * Check whether the slots buffer has capacity.
   * Returns null if ended/cancelled, true/false otherwise.
   * @returns {boolean | null}
   */
  [kCanWrite]() {
    if (this.#ended || this.#cancelled) return null;
    if (this.#bufferedBytes >= this.#options.budget) {
      return false;
    }
    return true;
  }

  // Private methods

  #notifyEndDrained() {
    if (this.#ended && this.#consumers.size === 0) {
      this[kOnEndDrained]?.();
    }
  }

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
        this.#bufferedBytes -= evicted.byteLength;
      }
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = this.#cachedMinCursor;

      if (this[kOnBufferDrained] &&
          this.#bufferedBytes < this.#options.budget) {
        this[kOnBufferDrained]();
      }
    }
  }

  #readEntry(entry) {
    try {
      return validateBatchEntry(entry);
    } catch (error) {
      this.#writer.fail(error);
      if (this.#error === undefined) this[kAbort](error);
      this.#buffer.clear();
      this.#bufferedBytes = 0;
      return null;
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
          const chunk = this.#readEntry(this.#buffer.get(bufferIndex));
          if (chunk === null) return;
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
  #state = 'open';
  #error;
  #pendingEnd;
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
      if (this.#state === 'open') {
        this.#resolvePendingDrains(true);
      }
    };
    this.#broadcast[kOnEndDrained] = () => this.#endDrained();
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

  get canWrite() {
    return this.#state === 'open' ? this.#broadcast[kCanWrite]() : null;
  }

  #canUseWriteFastPath(signal) {
    return !signal && this.#state === 'open' &&
        this.#broadcast[kCanWrite]();
  }

  write(chunk, options) {
    const converted = toWriterUint8Array(chunk);
    const signal = getWriterSignal(options);
    // Fast path: no signal, writer open, buffer has space
    if (this.#canUseWriteFastPath(signal)) {
      const batch = createBatchEntry([converted]);
      this.#broadcast[kWrite](batch);
      this.#totalBytes += batch.byteLength;
      return kResolvedPromise;
    }
    return this.#writeBatchSlow(createBatchEntry([converted]), signal);
  }

  writev(chunks, options) {
    const converted = convertChunks(chunks);
    const signal = getWriterSignal(options);
    const batch = createBatchEntry(converted);
    // Fast path: no signal, writer open, buffer has space
    if (this.#canUseWriteFastPath(signal)) {
      if (this.#state === 'open' && this.#broadcast[kWrite](batch)) {
        this.#totalBytes += batch.byteLength;
        return kResolvedPromise;
      }
      return this.#writeBatchSlow(batch, signal);
    }
    return this.#writeBatchSlow(batch, signal);
  }

  async #writeBatchSlow(batch, signal) {
    if (this.#state === 'errored') {
      throw this.#error;
    }
    if (this.#state !== 'open') {
      throw new ERR_INVALID_STATE.TypeError('Writer is closed');
    }

    signal?.throwIfAborted();

    if (this.#broadcast[kWrite](batch)) {
      this.#totalBytes += batch.byteLength;
      return;
    }

    const policy = this.#broadcast.backpressurePolicy;

    if (policy === 'strict') {
      if (this.#pendingWrites.length >= 1) {
        throw new ERR_INVALID_STATE.RangeError(
          'Backpressure violation: too many pending writes. ' +
          'Await each write() call to respect backpressure.');
      }
      return this.#createPendingWrite(batch, signal);
    }

    // 'unbounded' policy
    return this.#createPendingWrite(batch, signal);
  }

  writeSync(chunk) {
    const converted = toWriterUint8Array(chunk);
    if (this.#state !== 'open') return false;
    const batch = createBatchEntry([converted]);
    if (this.#broadcast[kWrite](batch)) {
      this.#totalBytes += batch.byteLength;
      return true;
    }
    return false;
  }

  writevSync(chunks) {
    const converted = convertChunks(chunks);
    if (this.#state !== 'open') return false;
    const batch = createBatchEntry(converted);
    if (this.#broadcast[kWrite](batch)) {
      this.#totalBytes += batch.byteLength;
      return true;
    }
    return false;
  }

  end(options) {
    const signal = getWriterSignal(options);
    if (this.#state === 'errored') return PromiseReject(this.#error);
    if (this.#state === 'closed') return PromiseResolve(this.#totalBytes);
    if (signal?.aborted) return PromiseReject(signal.reason);

    const endPromise = this.#getEndPromise();
    if (this.#state === 'open') {
      this.#state = 'closing';
      this.#resolvePendingDrains(false);
      this.#finishEndIfReady();
    }

    return raceEndWithSignal(endPromise, signal);
  }

  endSync() {
    if (this.#state === 'closed') return this.#totalBytes;
    if (this.#state === 'errored' || this.#state === 'closing') return -1;

    this.#state = 'closing';
    this.#resolvePendingDrains(false);
    this.#finishEndIfReady();
    return this.#state === 'closed' ? this.#totalBytes : -1;
  }

  fail(reason) {
    if (this.#state === 'errored' || this.#state === 'closed') return;
    this.#state = 'errored';
    const error = reason ?? new ERR_INVALID_STATE.TypeError('Failed');
    this.#error = error;
    this.#rejectPendingWrites(error);
    this.#rejectPendingDrains(error);
    this.#pendingEnd?.reject(error);
    this.#broadcast[kAbort](error);
  }

  [SymbolAsyncDispose]() {
    if (this.#state === 'closing') return this.#getEndPromise();
    this.fail();
    return PromiseResolve();
  }

  [SymbolDispose]() {
    this.fail();
  }

  [kCancelWriter]() {
    if (this.#state === 'closed' || this.#state === 'errored') return;
    this.#state = 'closed';
    this.#rejectPendingWrites(
      lazyDOMException('Broadcast cancelled', 'AbortError'));
    this.#resolvePendingDrains(false);
    this.#pendingEnd?.resolve(this.#totalBytes);
  }

  #getEndPromise() {
    this.#pendingEnd ??= PromiseWithResolvers();
    return this.#pendingEnd.promise;
  }

  #finishEndIfReady() {
    if (this.#state === 'closing' && this.#pendingWrites.length === 0) {
      this.#broadcast[kEnd]();
    }
  }

  #endDrained() {
    if (this.#state !== 'closing') return;
    this.#state = 'closed';
    this.#pendingEnd?.resolve(this.#totalBytes);
  }

  [kPendingWriteRemoved]() {
    this.#finishEndIfReady();
  }

  /**
   * Create a pending write promise, optionally racing against a signal.
   * If the signal fires, the entry is removed from pendingWrites and the
   * promise rejects. Signal listeners are cleaned up on normal resolution.
   * @returns {Promise<void>}
   */
  #createPendingWrite(batch, signal) {
    const { promise, resolve, reject } = PromiseWithResolvers();
    const entry = { __proto__: null, batch, resolve, reject };
    this.#pendingWrites.push(entry);
    if (signal) {
      wireBroadcastWriteSignal(entry, signal, resolve, reject, this);
    }
    return promise;
  }

  #resolvePendingWrites() {
    while (this.#pendingWrites.length > 0 && this.#broadcast[kCanWrite]()) {
      const pending = this.#pendingWrites.shift();
      try {
        validateBatchEntry(pending.batch);
        if (this.#broadcast[kWrite](pending.batch)) {
          this.#totalBytes += pending.batch.byteLength;
          pending.resolve();
        } else {
          this.#pendingWrites.unshift(pending);
          break;
        }
      } catch (error) {
        pending.reject(error);
      }
    }
    this.#finishEndIfReady();
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
    entry.batch = null;
    reject(signal.reason ?? lazyDOMException('Aborted', 'AbortError'));
    if (idx !== -1) self[kPendingWriteRemoved]();
  };
  entry.resolve = function() {
    signal.removeEventListener('abort', onAbort);
    entry.batch = null;
    resolve();
  };
  entry.reject = function(reason) {
    signal.removeEventListener('abort', onAbort);
    entry.batch = null;
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
  options = converters.BroadcastOptions(options, {
    __proto__: null,
    context: 'options',
  });
  const {
    budget = kMultiConsumerDefaultBudget,
    backpressure = 'strict',
    signal,
  } = options;
  validateInteger(budget, 'options.budget', 16384);

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

    options = converters.BroadcastOptions(options, {
      __proto__: null,
      context: 'options',
    });
    const result = broadcast(options);
    const { signal } = options;

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
