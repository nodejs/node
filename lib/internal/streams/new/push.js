'use strict';

// New Streams API - Push Stream Implementation
//
// Creates a bonded pair of writer and async iterable for push-based streaming
// with built-in backpressure.

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
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
} = require('internal/streams/new/utils');

const {
  pull: pullWithTransforms,
} = require('internal/streams/new/pull');

// =============================================================================
// PushQueue - Internal Queue with Chunk-Based Backpressure
// =============================================================================

class PushQueue {
  constructor(options = {}) {
    /** Buffered chunks (each slot is from one write/writev call) */
    this._slots = [];
    /** Pending writes waiting for buffer space */
    this._pendingWrites = [];
    /** Pending reads waiting for data */
    this._pendingReads = [];
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
    this._highWaterMark = options.highWaterMark ?? 1;
    this._backpressure = options.backpressure ?? 'strict';
    this._signal = options.signal;
    this._abortHandler = undefined;

    if (this._signal) {
      if (this._signal.aborted) {
        this.abort(this._signal.reason instanceof Error ?
          this._signal.reason :
          lazyDOMException('Aborted', 'AbortError'));
      } else {
        this._abortHandler = () => {
          this.abort(this._signal.reason instanceof Error ?
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
            ArrayPrototypeShift(this._slots);
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

    ArrayPrototypePush(this._slots, chunks);
    for (let i = 0; i < chunks.length; i++) {
      this._bytesWritten += chunks[i].byteLength;
    }

    this._resolvePendingReads();
    return true;
  }

  /**
   * Write chunks asynchronously.
   */
  async writeAsync(chunks) {
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
        return new Promise((resolve, reject) => {
          ArrayPrototypePush(this._pendingWrites,
                             { chunks, resolve, reject });
        });
      case 'block':
        return new Promise((resolve, reject) => {
          ArrayPrototypePush(this._pendingWrites,
                             { chunks, resolve, reject });
        });
      default:
        throw new ERR_INVALID_STATE(
          'Unexpected: writeSync should have handled non-strict policy');
    }
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
   * Signal error/abort.
   */
  abort(reason) {
    if (this._writerState === 'errored') return;

    this._writerState = 'errored';
    this._error = reason ?? new ERR_INVALID_STATE('Aborted');
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
      ArrayPrototypePush(this._pendingReads, { resolve, reject });
    });
  }

  consumerReturn() {
    if (this._consumerState !== 'active') return;
    this._consumerState = 'returned';
    this._cleanup();
    this._rejectPendingWrites(new ERR_INVALID_STATE('Stream closed by consumer'));
  }

  consumerThrow(error) {
    if (this._consumerState !== 'active') return;
    this._consumerState = 'thrown';
    this._error = error;
    this._cleanup();
    this._rejectPendingWrites(error);
  }

  // ===========================================================================
  // Private Methods
  // ===========================================================================

  _drain() {
    const result = [];
    for (let i = 0; i < this._slots.length; i++) {
      const slot = this._slots[i];
      for (let j = 0; j < slot.length; j++) {
        ArrayPrototypePush(result, slot[j]);
      }
    }
    this._slots = [];
    return result;
  }

  _resolvePendingReads() {
    while (this._pendingReads.length > 0) {
      if (this._slots.length > 0) {
        const pending = ArrayPrototypeShift(this._pendingReads);
        const result = this._drain();
        this._resolvePendingWrites();
        pending.resolve({ value: result, done: false });
      } else if (this._writerState === 'closed') {
        const pending = ArrayPrototypeShift(this._pendingReads);
        pending.resolve({ value: undefined, done: true });
      } else if (this._writerState === 'errored' && this._error) {
        const pending = ArrayPrototypeShift(this._pendingReads);
        pending.reject(this._error);
      } else {
        break;
      }
    }
  }

  _resolvePendingWrites() {
    while (this._pendingWrites.length > 0 &&
           this._slots.length < this._highWaterMark) {
      const pending = ArrayPrototypeShift(this._pendingWrites);
      ArrayPrototypePush(this._slots, pending.chunks);
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
    const reads = this._pendingReads;
    this._pendingReads = [];
    for (let i = 0; i < reads.length; i++) {
      reads[i].reject(error);
    }
  }

  _rejectPendingWrites(error) {
    const writes = this._pendingWrites;
    this._pendingWrites = [];
    for (let i = 0; i < writes.length; i++) {
      writes[i].reject(error);
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

  async write(chunk) {
    const bytes = toUint8Array(chunk);
    await this._queue.writeAsync([bytes]);
  }

  async writev(chunks) {
    const bytes = [];
    for (let i = 0; i < chunks.length; i++) {
      ArrayPrototypePush(bytes, toUint8Array(chunks[i]));
    }
    await this._queue.writeAsync(bytes);
  }

  writeSync(chunk) {
    if (!this._queue.canWriteSync()) return false;
    const bytes = toUint8Array(chunk);
    return this._queue.writeSync([bytes]);
  }

  writevSync(chunks) {
    if (!this._queue.canWriteSync()) return false;
    const bytes = [];
    for (let i = 0; i < chunks.length; i++) {
      ArrayPrototypePush(bytes, toUint8Array(chunks[i]));
    }
    return this._queue.writeSync(bytes);
  }

  async end() {
    return this._queue.end();
  }

  endSync() {
    return this._queue.end();
  }

  async abort(reason) {
    this._queue.abort(reason);
  }

  abortSync(reason) {
    this._queue.abort(reason);
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
