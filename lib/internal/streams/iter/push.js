'use strict';

// New Streams API - Push Stream Implementation
//
// Creates a bonded pair of writer and async iterable for push-based streaming
// with built-in backpressure.

const {
  ArrayIsArray,
  ArrayPrototypePush,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  Symbol,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { isError, lazyDOMException } = require('internal/util');
const {
  validateAbortSignal,
  validateInteger,
} = require('internal/validators');

const {
  drainableProtocol,
} = require('internal/streams/iter/types');

const {
  kPushDefaultBudget,
  kResolvedPromise,
  onSignalAbort,
  toUint8Array,
  convertChunks,
  getWriterSignal,
  parsePullArgs,
  validateBackpressure,
} = require('internal/streams/iter/utils');

const {
  pull: pullWithTransforms,
} = require('internal/streams/iter/pull');

const {
  RingBuffer,
} = require('internal/streams/iter/ringbuffer');

const kNoFailReason = Symbol('kNoFailReason');

// =============================================================================
// PushQueue - Internal Queue with Chunk-Based Backpressure
// =============================================================================

class PushQueue {
  /** Buffered chunks (each slot is from one write/writev call) */
  #slots = new RingBuffer();
  /** Pending writes waiting for buffer space */
  #pendingWrites = new RingBuffer();
  /** Pending reads waiting for data */
  #pendingReads = new RingBuffer();
  /** Pending drains waiting for backpressure to clear */
  #pendingDrains = [];
  /** Writer state: 'open' | 'closing' | 'closed' | 'errored' */
  #writerState = 'open';
  /** Consumer state: 'active' | 'returned' | 'thrown' */
  #consumerState = 'active';
  /** Error that closed the stream */
  #error = null;
  /** Total bytes written */
  #bytesWritten = 0;
  /** Pending end promise (resolves when consumer drains past end sentinel) */
  #pendingEnd = null;

  /** Configuration */
  #budget;
  #backpressure;
  #signal;
  #abortHandler;
  /** Cumulative byte size of buffered batches */
  #bufferedBytes = 0;

  constructor(options = { __proto__: null }) {
    const {
      budget = kPushDefaultBudget,
      backpressure = 'strict',
      signal,
    } = options;
    validateInteger(budget, 'options.budget', 16384);
    validateBackpressure(backpressure);
    if (signal !== undefined) {
      validateAbortSignal(signal, 'options.signal');
    }
    this.#budget = budget;
    this.#backpressure = backpressure;
    this.#signal = signal;
    this.#abortHandler = undefined;

    if (this.#signal) {
      this.#abortHandler = () => {
        this.fail(isError(this.#signal.reason) ?
          this.#signal.reason :
          lazyDOMException('Aborted', 'AbortError'));
      };
      onSignalAbort(this.#signal, this.#abortHandler);
    }
  }

  // ===========================================================================
  // Writer Methods
  // ===========================================================================

  /**
   * Check if the next write is likely to be accepted.
   * Returns null if writer is closed/errored or consumer has terminated.
   * @returns {boolean | null}
   */
  get canWrite() {
    if (this.#writerState !== 'open' || this.#consumerState !== 'active') {
      return null;
    }
    if ((this.#backpressure === 'strict' ||
          this.#backpressure === 'unbounded') &&
        this.#bufferedBytes >= this.#budget) {
      return false;
    }
    return true;
  }

  /**
   * Check if a sync write would be accepted.
   * @returns {boolean}
   */
  canWriteSync() {
    if (this.#writerState !== 'open') return false;
    if (this.#consumerState !== 'active') return false;
    if ((this.#backpressure === 'strict' ||
          this.#backpressure === 'unbounded') &&
        this.#bufferedBytes >= this.#budget) {
      return false;
    }
    return true;
  }

  /**
   * Write chunks synchronously if possible.
   * Returns true if write completed, false if buffer is full.
   * @returns {boolean}
   */
  writeSync(chunks) {
    if (this.#writerState !== 'open') return false;
    if (this.#consumerState !== 'active') return false;

    const batchSize = this.#batchByteSize(chunks);

    // Skip empty chunks -- zero-byte writes would accumulate infinitely
    // without ever triggering backpressure under a byte-budget model.
    if (batchSize === 0) return true;

    if (this.#bufferedBytes >= this.#budget) {
      switch (this.#backpressure) {
        case 'strict':
          return false;
        case 'unbounded':
          return false;
        case 'drop-oldest':
          while (this.#bufferedBytes >= this.#budget &&
                 this.#slots.length > 0) {
            const evicted = this.#slots.shift();
            this.#bufferedBytes -= this.#batchByteSize(evicted);
          }
          break;
        case 'drop-newest':
          // Discard this write, but return true
          this.#bytesWritten += batchSize;
          return true;
      }
    }

    this.#slots.push(chunks);
    this.#bufferedBytes += batchSize;
    this.#bytesWritten += batchSize;

    this.#resolvePendingReads();
    // After drop-oldest, evicting a large chunk may bring us under budget.
    // Resolve pending drains so writers waiting on backpressure can proceed.
    if (this.#bufferedBytes < this.#budget) {
      this.#resolvePendingDrains(true);
    }
    return true;
  }

  /**
   * Write chunks asynchronously.
   * If signal is provided, a write blocked on backpressure will reject
   * immediately when the signal fires. The cancelled write is removed from
   * pendingWrites so it does not occupy a slot. The queue itself is NOT put
   * into an error state - this is per-operation cancellation, not terminal
   * failure.
   * @returns {Promise<void>}
   */
  async writeAsync(chunks, signal) {
    // Check writer state before signal (spec order: state, then signal)
    if (this.#writerState === 'closed') {
      throw new ERR_INVALID_STATE.TypeError('Writer is closed');
    }
    if (this.#writerState === 'closing') {
      throw new ERR_INVALID_STATE.TypeError('Writer is closing');
    }
    if (this.#writerState === 'errored') {
      throw this.#error;
    }
    if (this.#consumerState !== 'active') {
      throw this.#consumerState === 'thrown' && this.#error ?
        this.#error :
        new ERR_INVALID_STATE.TypeError('Stream closed by consumer');
    }

    // Check for pre-aborted signal (after state checks per spec)
    signal?.throwIfAborted();

    // Try sync first
    if (this.writeSync(chunks)) {
      return;
    }

    // Buffer is full
    switch (this.#backpressure) {
      case 'strict':
        if (this.#pendingWrites.length >= 1) {
          throw new ERR_INVALID_STATE.RangeError(
            'Backpressure violation: too many pending writes. ' +
            'Await each write() call to respect backpressure.');
        }
        return this.#createPendingWrite(chunks, signal);
      case 'unbounded':
        return this.#createPendingWrite(chunks, signal);
      default:
        throw new ERR_INVALID_STATE(
          'Unexpected: writeSync should have handled non-strict policy');
    }
  }

  /**
   * Create a pending write promise, optionally racing against a signal.
   * If the signal fires, the entry is removed from pendingWrites and the
   * promise rejects. Signal listeners are cleaned up on normal resolution.
   * @returns {Promise<void>}
   */
  #createPendingWrite(chunks, signal) {
    const { promise, resolve, reject } = PromiseWithResolvers();
    const entry = { __proto__: null, chunks, resolve, reject };
    this.#pendingWrites.push(entry);

    if (signal) {
      const onAbort = () => {
        // Remove from queue so it doesn't occupy a slot
        const idx = this.#pendingWrites.indexOf(entry);
        if (idx !== -1) this.#pendingWrites.removeAt(idx);
        reject(signal.reason ?? lazyDOMException('Aborted', 'AbortError'));
      };

      // Wrap resolve/reject to clean up signal listener
      entry.resolve = function() {
        signal.removeEventListener('abort', onAbort);
        resolve();
      };
      entry.reject = function(reason) {
        signal.removeEventListener('abort', onAbort);
        reject(reason);
      };

      signal.addEventListener('abort', onAbort, { __proto__: null, once: true });
    }

    return promise;
  }

  /**
   * Signal end of stream. Returns total bytes written.
   * @returns {number}
   */
  end() {
    if (this.#writerState === 'errored') {
      return -2; // Signal to reject with stored error
    }
    if (this.#writerState === 'closing') {
      return -3; // Signal to PushWriter: wait for drain to complete
    }
    if (this.#writerState === 'closed') {
      return this.#bytesWritten; // Idempotent
    }

    this.#cleanup();
    this.#rejectPendingWrites(
      new ERR_INVALID_STATE.TypeError('Writer closed'));
    this.#resolvePendingDrains(false);

    // If buffer is empty, close immediately
    if (this.#slots.length === 0) {
      this.#writerState = 'closed';
      this.#resolvePendingReads();
      return this.#bytesWritten;
    }

    // Buffer has data: transition to closing, defer completion until drained
    this.#writerState = 'closing';
    return -3; // Signal to PushWriter: create deferred end promise
  }

  /**
   * Called by the read path when the consumer has drained all data while
   * the writer is in the 'closing' state. Transitions to 'closed' and
   * resolves the pending end promise.
   */
  endDrained() {
    if (this.#writerState !== 'closing') return;
    this.#writerState = 'closed';
    if (this.#pendingEnd) {
      this.#pendingEnd.resolve(this.#bytesWritten);
      this.#pendingEnd = null;
    }
  }

  /**
   * Put queue into terminal error state.
   * No-op if errored or closed (fully drained).
   * If closing (draining), short-circuits the drain.
   */
  fail(reason = kNoFailReason) {
    if (this.#writerState === 'errored' || this.#writerState === 'closed') {
      return;
    }

    const wasClosing = this.#writerState === 'closing';
    this.#writerState = 'errored';
    this.#error = reason === kNoFailReason ?
      new ERR_INVALID_STATE('Failed') :
      reason;
    this.#cleanup();
    this.#rejectPendingReads(this.#error);
    this.#rejectPendingDrains(this.#error);

    if (wasClosing) {
      // Short-circuit the graceful drain: reject the pending end promise
      if (this.#pendingEnd) {
        this.#pendingEnd.reject(this.#error);
        this.#pendingEnd = null;
      }
    } else {
      this.#rejectPendingWrites(this.#error);
    }
  }

  get totalBytesWritten() {
    return this.#bytesWritten;
  }

  get error() {
    return this.#error;
  }

  get backpressurePolicy() {
    return this.#backpressure;
  }

  get writerState() {
    return this.#writerState;
  }

  get pendingEndPromise() {
    return this.#pendingEnd?.promise ?? null;
  }

  setPendingEnd(pending) {
    this.#pendingEnd = pending;
  }

  /**
   * Wait for backpressure to clear (canWrite becomes true).
   * @returns {Promise<void>}
   */
  waitForDrain() {
    const { promise, resolve, reject } = PromiseWithResolvers();
    ArrayPrototypePush(this.#pendingDrains, { __proto__: null, resolve, reject });
    return promise;
  }

  // ===========================================================================
  // Consumer Methods
  // ===========================================================================

  async read() {
    // If there's data in the buffer, return it immediately
    if (this.#slots.length > 0) {
      const result = this.#drain();
      this.#resolvePendingWrites();
      // After draining, check if writer was closing and buffer is now empty
      if (this.#writerState === 'closing' && this.#slots.length === 0) {
        this.endDrained();
      }
      return { __proto__: null, done: false, value: result };
    }

    // Buffer empty and writer closing = drain complete
    if (this.#writerState === 'closing') {
      this.endDrained();
      return { __proto__: null, done: true, value: undefined };
    }

    if (this.#writerState === 'closed') {
      return { __proto__: null, done: true, value: undefined };
    }

    if (this.#writerState === 'errored') {
      throw this.#error;
    }

    const { promise, resolve, reject } = PromiseWithResolvers();
    this.#pendingReads.push({ __proto__: null, resolve, reject });
    return promise;
  }

  consumerReturn() {
    if (this.#consumerState !== 'active') return;
    this.#consumerState = 'returned';
    this.#cleanup();
    this.#resolvePendingReads();
    this.#rejectPendingWrites(
      new ERR_INVALID_STATE.TypeError('Stream closed by consumer'));
    // If closing, reject the pending end promise
    if (this.#writerState === 'closing' && this.#pendingEnd) {
      this.#pendingEnd.reject(
        new ERR_INVALID_STATE.TypeError('Stream closed by consumer'));
      this.#pendingEnd = null;
    }
    // Resolve pending drains with false - no more data will be consumed
    this.#resolvePendingDrains(false);
  }

  consumerThrow(error) {
    if (this.#consumerState !== 'active') return;
    this.#consumerState = 'thrown';
    this.#error = error;
    this.#cleanup();
    this.#rejectPendingReads(error);
    this.#rejectPendingWrites(error);
    if (this.#writerState === 'closing' && this.#pendingEnd) {
      this.#pendingEnd.reject(error);
      this.#pendingEnd = null;
    }
    // Reject pending drains - the consumer errored
    this.#rejectPendingDrains(error);
  }

  // ===========================================================================
  // Private Methods
  // ===========================================================================

  #drain() {
    this.#bufferedBytes = 0;
    if (this.#slots.length === 1) {
      return this.#slots.shift();
    }

    const result = [];
    for (let i = 0; i < this.#slots.length; i++) {
      const slot = this.#slots.get(i);
      for (let j = 0; j < slot.length; j++) {
        ArrayPrototypePush(result, slot[j]);
      }
    }
    this.#slots.clear();
    return result;
  }

  #batchByteSize(batch) {
    let size = 0;
    for (let i = 0; i < batch.length; i++) {
      size += TypedArrayPrototypeGetByteLength(batch[i]);
    }
    return size;
  }

  #resolvePendingReads() {
    while (this.#pendingReads.length > 0) {
      if (this.#slots.length > 0) {
        const pending = this.#pendingReads.shift();
        const result = this.#drain();
        this.#resolvePendingWrites();
        pending.resolve({ __proto__: null, done: false, value: result });
      } else if (this.#writerState === 'closing' && this.#slots.length === 0) {
        this.endDrained();
        const pending = this.#pendingReads.shift();
        pending.resolve({ __proto__: null, done: true, value: undefined });
      } else if (this.#writerState === 'closed') {
        const pending = this.#pendingReads.shift();
        pending.resolve({ __proto__: null, done: true, value: undefined });
      } else if (this.#writerState === 'errored') {
        const pending = this.#pendingReads.shift();
        pending.reject(this.#error);
      } else if (this.#consumerState === 'returned') {
        const pending = this.#pendingReads.shift();
        pending.resolve({ __proto__: null, done: true, value: undefined });
      } else {
        break;
      }
    }
  }

  #resolvePendingWrites() {
    while (this.#pendingWrites.length > 0 &&
          this.#bufferedBytes < this.#budget) {
      const pending = this.#pendingWrites.shift();
      const batchSize = this.#batchByteSize(pending.chunks);
      this.#slots.push(pending.chunks);
      this.#bufferedBytes += batchSize;
      this.#bytesWritten += batchSize;
      pending.resolve();
    }

    if (this.#bufferedBytes < this.#budget) {
      this.#resolvePendingDrains(true);
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

  #rejectPendingReads(error) {
    while (this.#pendingReads.length > 0) {
      this.#pendingReads.shift().reject(error);
    }
  }

  #rejectPendingWrites(error) {
    while (this.#pendingWrites.length > 0) {
      this.#pendingWrites.shift().reject(error);
    }
  }

  #cleanup() {
    if (this.#signal && this.#abortHandler) {
      this.#signal.removeEventListener('abort', this.#abortHandler);
      this.#abortHandler = undefined;
    }
  }
}

// =============================================================================
// PushWriter Implementation
// =============================================================================

class PushWriter {
  #queue;

  constructor(queue) {
    this.#queue = queue;
  }

  [drainableProtocol]() {
    const canWrite = this.canWrite;
    if (canWrite === null) return null;
    if (canWrite) return PromiseResolve(true);
    return this.#queue.waitForDrain();
  }

  get canWrite() {
    return this.#queue.canWrite;
  }

  write(chunk, options) {
    const signal = getWriterSignal(options);
    if (!signal && this.#queue.canWriteSync()) {
      const bytes = toUint8Array(chunk);
      this.#queue.writeSync([bytes]);
      return kResolvedPromise;
    }
    const bytes = toUint8Array(chunk);
    return this.#queue.writeAsync([bytes], signal);
  }

  writev(chunks, options) {
    if (!ArrayIsArray(chunks)) {
      throw new ERR_INVALID_ARG_TYPE('chunks', 'Array', chunks);
    }
    const signal = getWriterSignal(options);
    if (!signal && this.#queue.canWriteSync()) {
      const bytes = convertChunks(chunks);
      this.#queue.writeSync(bytes);
      return kResolvedPromise;
    }
    const bytes = convertChunks(chunks);
    return this.#queue.writeAsync(bytes, signal);
  }

  writeSync(chunk) {
    const bytes = toUint8Array(chunk);
    return this.#queue.writeSync([bytes]);
  }

  writevSync(chunks) {
    if (!ArrayIsArray(chunks)) {
      throw new ERR_INVALID_ARG_TYPE('chunks', 'Array', chunks);
    }
    const bytes = convertChunks(chunks);
    return this.#queue.writeSync(bytes);
  }

  end(options) {
    getWriterSignal(options);
    const result = this.#queue.end();
    if (result === -2) {
      // Errored: reject with stored error
      return PromiseReject(this.#queue.error);
    }
    if (result === -3) {
      // Closing: buffer has data, create deferred promise that resolves
      // when consumer drains past the end sentinel
      const pendingEndPromise = this.#queue.pendingEndPromise;
      if (pendingEndPromise !== null) {
        return pendingEndPromise;
      }
      const { promise, resolve, reject } = PromiseWithResolvers();
      this.#queue.setPendingEnd({ __proto__: null, promise, resolve, reject });
      return promise;
    }
    // >= 0: byte count (immediate close or idempotent)
    return PromiseResolve(result);
  }

  endSync() {
    const result = this.#queue.end();
    if (result === -2) return -1; // Errored
    if (result === -3) return -1; // Buffer not empty, can't wait
    return result;
  }

  fail(reason) {
    this.#queue.fail(arguments.length === 0 ? kNoFailReason : reason);
  }

  [SymbolAsyncDispose]() {
    const state = this.#queue.writerState;
    if (state === 'closing') {
      // Wait for graceful drain
      return this.#queue.pendingEndPromise ?? PromiseResolve();
    }
    if (state === 'open') {
      this.fail();
    }
    return PromiseResolve();
  }

  [SymbolDispose]() {
    this.fail();
  }
}

// =============================================================================
// Readable Implementation
// =============================================================================

function createReadable(queue) {
  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      return {
        __proto__: null,
        async next() {
          return queue.read();
        },
        async return() {
          queue.consumerReturn();
          return { __proto__: null, done: true, value: undefined };
        },
        async throw(error) {
          queue.consumerThrow(error);
          throw error;
        },
      };
    },
  };
}

// =============================================================================
// Stream.push() Factory
// =============================================================================

function parseArgs(args) {
  const result = parsePullArgs(args);
  // PushQueue constructor requires a non-undefined options object.
  if (result.options === undefined) {
    result.options = { __proto__: null };
  }
  return result;
}

/**
 * Create a push stream with optional transforms.
 * @param {...(Function|object)} args - Transforms, then options (optional)
 * @returns {{ writer: Writer, readable: AsyncIterable<Uint8Array[]> }}
 */
function push(...args) {
  const { transforms, options } = parseArgs(args);

  const queue = new PushQueue(options);
  const writer = new PushWriter(queue);
  const rawReadable = createReadable(queue);

  // Apply transforms lazily if provided
  let readable;
  if (transforms.length > 0) {
    if (options.signal) {
      readable = pullWithTransforms(
        rawReadable, ...transforms, { __proto__: null, signal: options.signal });
    } else {
      readable = pullWithTransforms(rawReadable, ...transforms);
    }
  } else {
    readable = rawReadable;
  }

  return { __proto__: null, writer, readable };
}

module.exports = {
  push,
};
