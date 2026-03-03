'use strict';

// New Streams API - Push Stream Implementation
//
// Creates a bonded pair of writer and async iterable for push-based streaming
// with built-in backpressure.

const {
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  Error,
  MathMax,
  Promise,
  PromiseResolve,
  SymbolAsyncIterator,
} = primordials;

const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');
const { lazyDOMException } = require('internal/util');

const {
  drainableProtocol,
} = require('internal/streams/new/types');

const {
  toUint8Array,
  allUint8Array,
} = require('internal/streams/new/utils');

const {
  pull: pullWithTransforms,
} = require('internal/streams/new/pull');

const {
  RingBuffer,
} = require('internal/streams/new/ringbuffer');

// Cached resolved promise to avoid allocating a new one on every sync fast-path.
const kResolvedPromise = PromiseResolve();

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
  /** Writer state: 'open' | 'closed' | 'errored' */
  #writerState = 'open';
  /** Consumer state: 'active' | 'returned' | 'thrown' */
  #consumerState = 'active';
  /** Error that closed the stream */
  #error = null;
  /** Total bytes written */
  #bytesWritten = 0;

  /** Configuration */
  #highWaterMark;
  #backpressure;
  #signal;
  #abortHandler;

  constructor(options = {}) {
    this.#highWaterMark = MathMax(1, options.highWaterMark ?? 1);
    this.#backpressure = options.backpressure ?? 'strict';
    this.#signal = options.signal;
    this.#abortHandler = undefined;

    if (this.#signal) {
      if (this.#signal.aborted) {
        this.fail(this.#signal.reason instanceof Error ?
          this.#signal.reason :
          lazyDOMException('Aborted', 'AbortError'));
      } else {
        this.#abortHandler = () => {
          this.fail(this.#signal.reason instanceof Error ?
            this.#signal.reason :
            lazyDOMException('Aborted', 'AbortError'));
        };
        this.#signal.addEventListener('abort', this.#abortHandler,
                                      { once: true });
      }
    }
  }

  // ===========================================================================
  // Writer Methods
  // ===========================================================================

  /**
   * Get slots available before hitting highWaterMark.
   * Returns null if writer is closed/errored or consumer has terminated.
   * @returns {number | null}
   */
  get desiredSize() {
    if (this.#writerState !== 'open' || this.#consumerState !== 'active') {
      return null;
    }
    return MathMax(0, this.#highWaterMark - this.#slots.length);
  }

  /**
   * Check if a sync write would be accepted.
   * @returns {boolean}
   */
  canWriteSync() {
    if (this.#writerState !== 'open') return false;
    if (this.#consumerState !== 'active') return false;
    if ((this.#backpressure === 'strict' ||
         this.#backpressure === 'block') &&
        this.#slots.length >= this.#highWaterMark) {
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

    if (this.#slots.length >= this.#highWaterMark) {
      switch (this.#backpressure) {
        case 'strict':
        case 'block':
          return false;
        case 'drop-oldest':
          if (this.#slots.length > 0) {
            this.#slots.shift();
          }
          break;
        case 'drop-newest':
          // Discard this write, but return true
          for (let i = 0; i < chunks.length; i++) {
            this.#bytesWritten += chunks[i].byteLength;
          }
          return true;
      }
    }

    this.#slots.push(chunks);
    for (let i = 0; i < chunks.length; i++) {
      this.#bytesWritten += chunks[i].byteLength;
    }

    this.#resolvePendingReads();
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
    // Check for pre-aborted signal
    if (signal?.aborted) {
      throw signal.reason ?? lazyDOMException('Aborted', 'AbortError');
    }

    if (this.#writerState !== 'open') {
      throw new ERR_INVALID_STATE('Writer is closed');
    }
    if (this.#consumerState !== 'active') {
      throw this.#consumerState === 'thrown' && this.#error ?
        this.#error :
        new ERR_INVALID_STATE('Stream closed by consumer');
    }

    // Try sync first
    if (this.writeSync(chunks)) {
      return;
    }

    // Buffer is full
    switch (this.#backpressure) {
      case 'strict':
        if (this.#pendingWrites.length >= this.#highWaterMark) {
          throw new ERR_INVALID_STATE(
            'Backpressure violation: too many pending writes. ' +
            'Await each write() call to respect backpressure.');
        }
        return this.#createPendingWrite(chunks, signal);
      case 'block':
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
   */
  #createPendingWrite(chunks, signal) {
    return new Promise((resolve, reject) => {
      const entry = { chunks, resolve, reject };
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

  /**
   * Signal end of stream. Returns total bytes written.
   * @returns {number}
   */
  end() {
    if (this.#writerState !== 'open') {
      return this.#bytesWritten;
    }

    this.#writerState = 'closed';
    this.#cleanup();
    this.#resolvePendingReads();
    this.#rejectPendingWrites(new ERR_INVALID_STATE('Writer closed'));
    this.#resolvePendingDrains(false);
    return this.#bytesWritten;
  }

  /**
   * Put queue into terminal error state.
   */
  fail(reason) {
    if (this.#writerState === 'errored') return;

    this.#writerState = 'errored';
    this.#error = reason ?? new ERR_INVALID_STATE('Failed');
    this.#cleanup();
    this.#rejectPendingReads(this.#error);
    this.#rejectPendingWrites(this.#error);
    this.#rejectPendingDrains(this.#error);
  }

  get totalBytesWritten() {
    return this.#bytesWritten;
  }

  /**
   * Wait for backpressure to clear (desiredSize > 0).
   */
  waitForDrain() {
    return new Promise((resolve, reject) => {
      ArrayPrototypePush(this.#pendingDrains, { resolve, reject });
    });
  }

  // ===========================================================================
  // Consumer Methods
  // ===========================================================================

  async read() {
    // If there's data in the buffer, return it immediately
    if (this.#slots.length > 0) {
      const result = this.#drain();
      this.#resolvePendingWrites();
      return { __proto__: null, value: result, done: false };
    }

    if (this.#writerState === 'closed') {
      return { __proto__: null, value: undefined, done: true };
    }

    if (this.#writerState === 'errored' && this.#error) {
      throw this.#error;
    }

    return new Promise((resolve, reject) => {
      this.#pendingReads.push({ resolve, reject });
    });
  }

  consumerReturn() {
    if (this.#consumerState !== 'active') return;
    this.#consumerState = 'returned';
    this.#cleanup();
    this.#rejectPendingWrites(
      new ERR_INVALID_STATE('Stream closed by consumer'));
    // Resolve pending drains with false - no more data will be consumed
    this.#resolvePendingDrains(false);
  }

  consumerThrow(error) {
    if (this.#consumerState !== 'active') return;
    this.#consumerState = 'thrown';
    this.#error = error;
    this.#cleanup();
    this.#rejectPendingWrites(error);
    // Reject pending drains - the consumer errored
    this.#rejectPendingDrains(error);
  }

  // ===========================================================================
  // Private Methods
  // ===========================================================================

  #drain() {
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

  #resolvePendingReads() {
    while (this.#pendingReads.length > 0) {
      if (this.#slots.length > 0) {
        const pending = this.#pendingReads.shift();
        const result = this.#drain();
        this.#resolvePendingWrites();
        pending.resolve({ value: result, done: false });
      } else if (this.#writerState === 'closed') {
        const pending = this.#pendingReads.shift();
        pending.resolve({ value: undefined, done: true });
      } else if (this.#writerState === 'errored' && this.#error) {
        const pending = this.#pendingReads.shift();
        pending.reject(this.#error);
      } else {
        break;
      }
    }
  }

  #resolvePendingWrites() {
    while (this.#pendingWrites.length > 0 &&
           this.#slots.length < this.#highWaterMark) {
      const pending = this.#pendingWrites.shift();
      this.#slots.push(pending.chunks);
      for (let i = 0; i < pending.chunks.length; i++) {
        this.#bytesWritten += pending.chunks[i].byteLength;
      }
      pending.resolve();
    }

    if (this.#slots.length < this.#highWaterMark) {
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
    const desired = this.desiredSize;
    if (desired === null) return null;
    if (desired > 0) return PromiseResolve(true);
    return this.#queue.waitForDrain();
  }

  get desiredSize() {
    return this.#queue.desiredSize;
  }

  write(chunk, options) {
    if (!options?.signal && this.#queue.canWriteSync()) {
      const bytes = toUint8Array(chunk);
      this.#queue.writeSync([bytes]);
      return kResolvedPromise;
    }
    const bytes = toUint8Array(chunk);
    return this.#queue.writeAsync([bytes], options?.signal);
  }

  writev(chunks, options) {
    if (!options?.signal && this.#queue.canWriteSync()) {
      let bytes;
      if (allUint8Array(chunks)) {
        bytes = ArrayPrototypeSlice(chunks);
      } else {
        bytes = [];
        for (let i = 0; i < chunks.length; i++) {
          ArrayPrototypePush(bytes, toUint8Array(chunks[i]));
        }
      }
      this.#queue.writeSync(bytes);
      return kResolvedPromise;
    }
    let bytes;
    if (allUint8Array(chunks)) {
      bytes = ArrayPrototypeSlice(chunks);
    } else {
      bytes = [];
      for (let i = 0; i < chunks.length; i++) {
        ArrayPrototypePush(bytes, toUint8Array(chunks[i]));
      }
    }
    return this.#queue.writeAsync(bytes, options?.signal);
  }

  writeSync(chunk) {
    if (!this.#queue.canWriteSync()) return false;
    const bytes = toUint8Array(chunk);
    return this.#queue.writeSync([bytes]);
  }

  writevSync(chunks) {
    if (!this.#queue.canWriteSync()) return false;
    let bytes;
    if (allUint8Array(chunks)) {
      bytes = ArrayPrototypeSlice(chunks);
    } else {
      bytes = [];
      for (let i = 0; i < chunks.length; i++) {
        ArrayPrototypePush(bytes, toUint8Array(chunks[i]));
      }
    }
    return this.#queue.writeSync(bytes);
  }

  end(options) {
    // end() on PushQueue is synchronous (sets state, resolves pending reads).
    // Signal accepted for interface compliance but there is nothing to cancel.
    return PromiseResolve(this.#queue.end());
  }

  endSync() {
    return this.#queue.end();
  }

  fail(reason) {
    this.#queue.fail(reason);
    return kResolvedPromise;
  }

  failSync(reason) {
    this.#queue.fail(reason);
    return true;
  }
}

// =============================================================================
// Readable Implementation
// =============================================================================

function createReadable(queue) {
  return {
    [SymbolAsyncIterator]() {
      return {
        async next() {
          return queue.read();
        },
        async return() {
          queue.consumerReturn();
          return { __proto__: null, value: undefined, done: true };
        },
        async throw(error) {
          queue.consumerThrow(error);
          return { __proto__: null, value: undefined, done: true };
        },
      };
    },
  };
}

// =============================================================================
// Stream.push() Factory
// =============================================================================

function isOptions(arg) {
  return (
    typeof arg === 'object' &&
    arg !== null &&
    !('transform' in arg)
  );
}

function parseArgs(args) {
  if (args.length === 0) {
    return { transforms: [], options: {} };
  }

  const last = args[args.length - 1];
  if (isOptions(last)) {
    return {
      transforms: ArrayPrototypeSlice(args, 0, -1),
      options: last,
    };
  }

  return {
    transforms: args,
    options: {},
  };
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
        rawReadable, ...transforms, { signal: options.signal });
    } else {
      readable = pullWithTransforms(rawReadable, ...transforms);
    }
  } else {
    readable = rawReadable;
  }

  return { writer, readable };
}

module.exports = {
  push,
};
