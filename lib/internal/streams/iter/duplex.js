'use strict';

// New Streams API - Duplex Channel
//
// Creates a pair of connected channels where data written to one
// channel's writer appears in the other channel's readable.

const {
  SymbolAsyncDispose,
  SymbolAsyncIterator,
} = primordials;

const {
  push,
} = require('internal/streams/iter/push');
const {
  validateAbortSignal,
  validateObject,
} = require('internal/validators');

/**
 * Create a pair of connected duplex channels for bidirectional communication.
 * @param {{ highWaterMark?: number, backpressure?: string, signal?: AbortSignal,
 *           a?: object, b?: object }} [options]
 * @returns {[DuplexChannel, DuplexChannel]}
 */
function duplex(options = { __proto__: null }) {
  validateObject(options, 'options');
  const { highWaterMark, backpressure, signal, a, b } = options;
  if (a !== undefined) {
    validateObject(a, 'options.a');
  }
  if (b !== undefined) {
    validateObject(b, 'options.b');
  }
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
  }

  // Channel A writes to B's readable (A->B direction).
  // Signal is NOT passed to push() -- we handle abort via close() below.
  const { writer: aWriter, readable: bReadable } = push({
    highWaterMark: a?.highWaterMark ?? highWaterMark,
    backpressure: a?.backpressure ?? backpressure,
  });

  // Channel B writes to A's readable (B->A direction)
  const { writer: bWriter, readable: aReadable } = push({
    highWaterMark: b?.highWaterMark ?? highWaterMark,
    backpressure: b?.backpressure ?? backpressure,
  });

  let aClosed = false;
  let bClosed = false;
  // Track active iterators so close() can call .return() on them
  let aReadableIterator = null;
  let bReadableIterator = null;

  const channelA = {
    __proto__: null,
    get writer() { return aWriter; },
    // Wrap readable to track the iterator for cleanup on close()
    get readable() {
      return {
        __proto__: null,
        [SymbolAsyncIterator]() {
          const iter = aReadable[SymbolAsyncIterator]();
          aReadableIterator = iter;
          return iter;
        },
      };
    },
    async close() {
      if (aClosed) return;
      aClosed = true;
      // End the writer (signals end-of-stream to B's readable)
      if (aWriter.endSync() < 0) {
        await aWriter.end();
      }
      // Stop iteration of this channel's readable
      if (aReadableIterator?.return) {
        await aReadableIterator.return();
        aReadableIterator = null;
      }
    },
    [SymbolAsyncDispose]() {
      return this.close();
    },
  };

  const channelB = {
    __proto__: null,
    get writer() { return bWriter; },
    get readable() {
      return {
        __proto__: null,
        [SymbolAsyncIterator]() {
          const iter = bReadable[SymbolAsyncIterator]();
          bReadableIterator = iter;
          return iter;
        },
      };
    },
    async close() {
      if (bClosed) return;
      bClosed = true;
      if (bWriter.endSync() < 0) {
        await bWriter.end();
      }
      if (bReadableIterator?.return) {
        await bReadableIterator.return();
        bReadableIterator = null;
      }
    },
    [SymbolAsyncDispose]() {
      return this.close();
    },
  };

  // Signal handler: fail both writers with the abort reason so consumers
  // see the error. This is an error-path shutdown, not a clean close.
  if (signal) {
    const abortBoth = () => {
      const reason = signal.reason;
      aWriter.fail(reason);
      bWriter.fail(reason);
    };
    if (signal.aborted) {
      abortBoth();
    } else {
      signal.addEventListener('abort', abortBoth,
                              { __proto__: null, once: true });
    }
  }

  return [channelA, channelB];
}

module.exports = {
  duplex,
};
