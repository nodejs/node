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
  constructor(options = {}) {
    /** Buffered chunks (each slot is from one write/writev call) */
    this._slots = new RingBuffer();
    /** Pending writes waiting for buffer space */
    this._pendingWrites = new RingBuffer();
    /** Pending reads waiting for data */
    this._pendingReads = new RingBuffer();
    /** Pending drains waiting for backpressure to clear */
    this._pendingDrains = [];
    /** Writer state: 'open' | 'closed' | 'errored' */
    this._writerState = 'open';
    /** Consumer state: 'active' | 'returned' | 'thrown' */
    this._consumerState = 'active';
    /** Error that closed the stream */
    this._error = null;
    /** Total bytes written */
    this._bytesWritten = 0;

    /** Configuration */
    this._highWaterMark = MathMax(1, options.highWaterMark ?? 1);
    this._backpressure = options.backpressure ?? 'strict';
    this._signal = options.signal;
    this._abortHandler = undefined;

    if (this._signal) {
      if (this._signal.aborted) {
        this.fail(this._signal.reason instanceof Error ?
          this._signal.reason :
          lazyDOMException('Aborted', 'AbortError'));
      } else {
        this._abortHandler = () => {
          this.fail(this._signal.reason instanceof Error ?
            this._signal.reason :
            lazyDOMException('Aborted', 'AbortError'));
        };
        this._signal.addEventListener('abort', this._abortHandler,
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
    if (this._writerState !== 'open' || this._consumerState !== 'active') {
      return null;
    }
    return MathMax(0, this._highWaterMark - this._slots.length);
  }

  /**
   * Check if a sync write would be accepted.
   * @returns {boolean}
   */
  canWriteSync() {
    if (this._writerState !== 'open') return false;
    if (this._consumerState !== 'active') return false;
    if ((this._backpressure === 'strict' ||
         this._backpressure === 'block') &&
        this._slots.length >= this._highWaterMark) {
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
    if (this._writerState !== 'open') return false;
    if (this._consumerState !== 'active') return false;

    if (this._slots.length >= this._highWaterMark) {
      switch (this._backpressure) {
        case 'strict':
        case 'block':
          return false;
        case 'drop-oldest':
          if (this._slots.length > 0) {
            this._slots.shift();
          }
          break;
        case 'drop-newest':
          // Discard this write, but return true
          for (let i = 0; i < chunks.length; i++) {
            this._bytesWritten += chunks[i].byteLength;
          }
          return true;
      }
    }

    this._slots.push(chunks);
    for (let i = 0; i < chunks.length; i++) {
      this._bytesWritten += chunks[i].byteLength;
    }

    this._resolvePendingReads();
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

    if (this._writerState !== 'open') {
      throw new ERR_INVALID_STATE('Writer is closed');
    }
    if (this._consumerState !== 'active') {
      throw this._consumerState === 'thrown' && this._error ?
        this._error :
        new ERR_INVALID_STATE('Stream closed by consumer');
    }

    // Try sync first
    if (this.writeSync(chunks)) {
      return;
    }

    // Buffer is full
    switch (this._backpressure) {
      case 'strict':
        if (this._pendingWrites.length >= this._highWaterMark) {
          throw new ERR_INVALID_STATE(
            'Backpressure violation: too many pending writes. ' +
            'Await each write() call to respect backpressure.');
        }
        return this._createPendingWrite(chunks, signal);
      case 'block':
        return this._createPendingWrite(chunks, signal);
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
  _createPendingWrite(chunks, signal) {
    return new Promise((resolve, reject) => {
      const entry = { chunks, resolve, reject };
      this._pendingWrites.push(entry);

      if (!signal) return;

      const onAbort = () => {
        // Remove from queue so it doesn't occupy a slot
        const idx = this._pendingWrites.indexOf(entry);
        if (idx !== -1) this._pendingWrites.removeAt(idx);
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
    if (this._writerState !== 'open') {
      return this._bytesWritten;
    }

    this._writerState = 'closed';
    this._cleanup();
    this._resolvePendingReads();
    this._rejectPendingWrites(new ERR_INVALID_STATE('Writer closed'));
    this._resolvePendingDrains(false);
    return this._bytesWritten;
  }

  /**
   * Put queue into terminal error state.
   */
  fail(reason) {
    if (this._writerState === 'errored') return;

    this._writerState = 'errored';
    this._error = reason ?? new ERR_INVALID_STATE('Failed');
    this._cleanup();
    this._rejectPendingReads(this._error);
    this._rejectPendingWrites(this._error);
    this._rejectPendingDrains(this._error);
  }

  get totalBytesWritten() {
    return this._bytesWritten;
  }

  /**
   * Wait for backpressure to clear (desiredSize > 0).
   */
  waitForDrain() {
    return new Promise((resolve, reject) => {
      ArrayPrototypePush(this._pendingDrains, { resolve, reject });
    });
  }

  // ===========================================================================
  // Consumer Methods
  // ===========================================================================

  async read() {
    // If there's data in the buffer, return it immediately
    if (this._slots.length > 0) {
      const result = this._drain();
      this._resolvePendingWrites();
      return { __proto__: null, value: result, done: false };
    }

    if (this._writerState === 'closed') {
      return { __proto__: null, value: undefined, done: true };
    }

    if (this._writerState === 'errored' && this._error) {
      throw this._error;
    }

    return new Promise((resolve, reject) => {
      this._pendingReads.push({ resolve, reject });
    });
  }

  consumerReturn() {
    if (this._consumerState !== 'active') return;
    this._consumerState = 'returned';
    this._cleanup();
    this._rejectPendingWrites(new ERR_INVALID_STATE('Stream closed by consumer'));
    // Resolve pending drains with false - no more data will be consumed
    this._resolvePendingDrains(false);
  }

  consumerThrow(error) {
    if (this._consumerState !== 'active') return;
    this._consumerState = 'thrown';
    this._error = error;
    this._cleanup();
    this._rejectPendingWrites(error);
    // Reject pending drains - the consumer errored
    this._rejectPendingDrains(error);
  }

  // ===========================================================================
  // Private Methods
  // ===========================================================================

  _drain() {
    const result = [];
    for (let i = 0; i < this._slots.length; i++) {
      const slot = this._slots.get(i);
      for (let j = 0; j < slot.length; j++) {
        ArrayPrototypePush(result, slot[j]);
      }
    }
    this._slots.clear();
    return result;
  }

  _resolvePendingReads() {
    while (this._pendingReads.length > 0) {
      if (this._slots.length > 0) {
        const pending = this._pendingReads.shift();
        const result = this._drain();
        this._resolvePendingWrites();
        pending.resolve({ value: result, done: false });
      } else if (this._writerState === 'closed') {
        const pending = this._pendingReads.shift();
        pending.resolve({ value: undefined, done: true });
      } else if (this._writerState === 'errored' && this._error) {
        const pending = this._pendingReads.shift();
        pending.reject(this._error);
      } else {
        break;
      }
    }
  }

  _resolvePendingWrites() {
    while (this._pendingWrites.length > 0 &&
           this._slots.length < this._highWaterMark) {
      const pending = this._pendingWrites.shift();
      this._slots.push(pending.chunks);
      for (let i = 0; i < pending.chunks.length; i++) {
        this._bytesWritten += pending.chunks[i].byteLength;
      }
      pending.resolve();
    }

    if (this._slots.length < this._highWaterMark) {
      this._resolvePendingDrains(true);
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

  _rejectPendingReads(error) {
    while (this._pendingReads.length > 0) {
      this._pendingReads.shift().reject(error);
    }
  }

  _rejectPendingWrites(error) {
    while (this._pendingWrites.length > 0) {
      this._pendingWrites.shift().reject(error);
    }
  }

  _cleanup() {
    if (this._signal && this._abortHandler) {
      this._signal.removeEventListener('abort', this._abortHandler);
      this._abortHandler = undefined;
    }
  }
}

// =============================================================================
// PushWriter Implementation
// =============================================================================

class PushWriter {
  constructor(queue) {
    this._queue = queue;
  }

  [drainableProtocol]() {
    const desired = this.desiredSize;
    if (desired === null) return null;
    if (desired > 0) return PromiseResolve(true);
    return this._queue.waitForDrain();
  }

  get desiredSize() {
    return this._queue.desiredSize;
  }

  write(chunk, options) {
    if (!options?.signal && this._queue.canWriteSync()) {
      const bytes = toUint8Array(chunk);
      this._queue.writeSync([bytes]);
      return kResolvedPromise;
    }
    const bytes = toUint8Array(chunk);
    return this._queue.writeAsync([bytes], options?.signal);
  }

  writev(chunks, options) {
    if (!options?.signal && this._queue.canWriteSync()) {
      let bytes;
      if (allUint8Array(chunks)) {
        bytes = ArrayPrototypeSlice(chunks);
      } else {
        bytes = [];
        for (let i = 0; i < chunks.length; i++) {
          ArrayPrototypePush(bytes, toUint8Array(chunks[i]));
        }
      }
      this._queue.writeSync(bytes);
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
    return this._queue.writeAsync(bytes, options?.signal);
  }

  writeSync(chunk) {
    if (!this._queue.canWriteSync()) return false;
    const bytes = toUint8Array(chunk);
    return this._queue.writeSync([bytes]);
  }

  writevSync(chunks) {
    if (!this._queue.canWriteSync()) return false;
    let bytes;
    if (allUint8Array(chunks)) {
      bytes = ArrayPrototypeSlice(chunks);
    } else {
      bytes = [];
      for (let i = 0; i < chunks.length; i++) {
        ArrayPrototypePush(bytes, toUint8Array(chunks[i]));
      }
    }
    return this._queue.writeSync(bytes);
  }

  end(options) {
    // end() on PushQueue is synchronous (sets state, resolves pending reads).
    // Signal accepted for interface compliance but there is nothing to cancel.
    return PromiseResolve(this._queue.end());
  }

  endSync() {
    return this._queue.end();
  }

  fail(reason) {
    this._queue.fail(reason);
    return kResolvedPromise;
  }

  failSync(reason) {
    this._queue.fail(reason);
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
