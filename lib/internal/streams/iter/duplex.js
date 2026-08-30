'use strict';

// New Streams API - Duplex Channel
//
// Creates a pair of connected channels where data written to one
// channel's writer appears in the other channel's readable.

const {
  SafePromiseAllReturnVoid,
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
 * @param {{ budget?: number, backpressure?: string, signal?: AbortSignal,
 *           a?: object, b?: object }} [options]
 * @returns {[DuplexChannel, DuplexChannel]}
 */
function duplex(options = { __proto__: null }) {
  validateObject(options, 'options');
  const { budget, backpressure, signal, a, b } = options;
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
    budget: a?.budget ?? budget,
    backpressure: a?.backpressure ?? backpressure,
  });

  // Channel B writes to A's readable (B->A direction)
  const { writer: bWriter, readable: aReadable } = push({
    budget: b?.budget ?? budget,
    backpressure: b?.backpressure ?? backpressure,
  });

  const channelA = createDuplexChannel(aWriter, aReadable);
  const channelB = createDuplexChannel(bWriter, bReadable);

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

function createDuplexChannel(writer, readable) {
  // A push readable has one shared consumer state. Keeping an iterator from
  // creation lets close() terminate that state even if no caller has iterated.
  const closeIterator = readable[SymbolAsyncIterator]();
  let closePromise;

  return {
    __proto__: null,
    get writer() { return writer; },
    get readable() { return readable; },
    close() {
      closePromise ??= closeDuplexChannel(writer, closeIterator);
      return closePromise;
    },
    [SymbolAsyncDispose]() {
      return this.close();
    },
  };
}

async function closeDuplexChannel(writer, closeIterator) {
  const result = writer.endSync();
  const endPromise = result < 0 ? writer.end() : undefined;
  const returnPromise = closeIterator.return();

  if (endPromise !== undefined) {
    await SafePromiseAllReturnVoid([endPromise, returnPromise]);
  } else {
    await returnPromise;
  }
}

module.exports = {
  duplex,
};
