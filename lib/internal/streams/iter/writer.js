'use strict';

// TODO(@jasnell) Temporarily ignoring c8 coverage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  PromisePrototypeThen,
  PromiseResolve,
  PromiseWithResolvers,
  SymbolAsyncDispose,
  SymbolDispose,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  drainableProtocol,
} = require('internal/streams/iter/types');

const {
  convertChunks,
  getWriterSignal,
  toWriterUint8Array,
} = require('internal/streams/iter/utils');

const {
  markPromiseAsHandled,
} = internalBinding('util');

const kEmptyObject = { __proto__: null };

const {
  codes: {
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

function getWriter(obj) {
  const {
    setDrainCallback,
    setStopSendingCallback,
    writeDesiredSize,
    writeEnded,
    writeChunks,
    resetStream,
    endWrite,
    nonWritableStream,
    initStreamingSource,
  } = obj;
  // TODO, if this is not an internal function,
  // the input should be checked for types
  let closed = false;
  let ending = false;
  let errored = false;
  let error = null;
  let totalBytesWritten = 0;
  let drainWakeup = null;
  let pendingWrite = null;
  let pendingEnd = null;

  function raceWithSignal(promise, signal) {
    if (signal === undefined) return promise;
    signal.throwIfAborted();

    const deferred = PromiseWithResolvers();
    const onAbort = () => deferred.reject(signal.reason);
    signal.addEventListener('abort', onAbort, {
      __proto__: null,
      once: true,
    });
    PromisePrototypeThen(
      promise,
      (value) => {
        signal.removeEventListener('abort', onAbort);
        deferred.resolve(value);
      },
      (reason) => {
        signal.removeEventListener('abort', onAbort);
        deferred.reject(reason);
      });
    return deferred.promise;
  }

  function waitForDrain(signal) {
    drainWakeup ??= PromiseWithResolvers();
    return raceWithSignal(drainWakeup.promise, signal);
  }

  // Drain callback - The implementation C++/js fires this when send buffer has space
  setDrainCallback(() => {
    if (drainWakeup) {
      drainWakeup.resolve(true);
      drainWakeup = null;
    }
  });

  setStopSendingCallback((reason) => {
    if (!closed && !errored) {
      errored = true;
      error = reason;
      if (drainWakeup != null) {
        markPromiseAsHandled(drainWakeup.promise);
        drainWakeup.reject(error);
        drainWakeup = null;
      }
    }
  });

  // A note on backpressure handling: per the stream/iter spec, the default
  // backpressure policy for writers is strict. One async write may wait for
  // capacity; additional writes are rejected until it settles.

  function writeConvertedSync(chunk, token) {
    const isPending = pendingWrite === token && token !== undefined;
    // If the stream is closed, errored, or write-ended, we cannot accept
    // more data. Refuse the sync write.
    if (closed || errored || writeEnded() ||
        (ending && !isPending) ||
        (pendingWrite !== null && !isPending)) {
      return false;
    }
    const len = TypedArrayPrototypeGetByteLength(chunk);
    if (len === 0) return true;
    // Refuse the write only when there is no available capacity at
    // all. If we can write we allow the write even if the
    // chunk is larger than the remaining capacity -- the source
    // will accept the data into the underlying queues e.g. DataQueue and
    // UpdateWriteDesiredSize() will drop writeDesiredSize toward 0,
    // at which point the standard drain mechanism takes over.
    // This follows the iter-streams model where writes beyond the
    // budget succeed and backpressure applies to *subsequent* writes.
    if (writeDesiredSize() === 0) return false;
    const result = writeChunks([chunk]);
    if (result === undefined) return false;
    totalBytesWritten += len;
    return true;
  }

  function writeSync(chunk) {
    return writeConvertedSync(toWriterUint8Array(chunk));
  }

  function write(chunk, options = kEmptyObject) {
    chunk = toWriterUint8Array(chunk);
    const signal = getWriterSignal(options);
    return writeAsync(chunk, signal);
  }

  async function writeAsync(chunk, signal) {
    if (errored) throw error;
    if (closed || ending || writeEnded()) {
      throw new ERR_INVALID_STATE('Writer is closed');
    }
    signal?.throwIfAborted();

    if (writeConvertedSync(chunk)) return;
    await writeWhenDrained(chunk, writeConvertedSync, signal);
  }

  async function writeWhenDrained(chunks, writeConverted, signal) {
    if (pendingWrite !== null) {
      throw new ERR_INVALID_STATE.RangeError(
        'Backpressure violation: too many pending writes. ' +
        'Await each write() call to respect backpressure.');
    }
    if (writeDesiredSize() !== 0) {
      throw new ERR_INVALID_STATE('Stream write buffer is full');
    }

    const done = PromiseWithResolvers();
    const token = { __proto__: null, done };
    pendingWrite = token;
    try {
      while (true) {
        await waitForDrain(signal);
        if (errored) throw error;
        if (closed || writeEnded()) {
          throw new ERR_INVALID_STATE('Writer is closed');
        }
        signal?.throwIfAborted();
        if (writeConverted(chunks, token)) return;
        if (writeDesiredSize() !== 0) {
          throw new ERR_INVALID_STATE('Stream write buffer is full');
        }
      }
    } finally {
      if (pendingWrite === token) pendingWrite = null;
      done.resolve();
    }
  }

  function writevConvertedSync(chunks, token) {
    const isPending = pendingWrite === token && token !== undefined;
    if (closed || errored || writeEnded() ||
        (ending && !isPending) ||
        (pendingWrite !== null && !isPending)) {
      return false;
    }
    let len = 0;
    for (const c of chunks) len += TypedArrayPrototypeGetByteLength(c);
    if (len === 0) return true;
    if (writeDesiredSize() === 0) return false;
    const result = writeChunks(chunks);
    if (result === undefined) return false;
    totalBytesWritten += len;
    return true;
  }

  function writevSync(chunks) {
    return writevConvertedSync(convertChunks(chunks));
  }

  function writev(chunks, options = kEmptyObject) {
    chunks = convertChunks(chunks);
    const signal = getWriterSignal(options);
    return writevAsync(chunks, signal);
  }

  async function writevAsync(chunks, signal) {
    if (errored) throw error;
    if (closed || ending || writeEnded()) {
      throw new ERR_INVALID_STATE('Writer is closed');
    }
    signal?.throwIfAborted();

    if (writevConvertedSync(chunks)) return;
    await writeWhenDrained(chunks, writevConvertedSync, signal);
  }

  function endSync() {
    // Per the streams/iter spec, endSync and end follow a try-fallback
    // pattern. That is, callers should try endSync first and if it returns
    // -1, then they should call and await end(). This is a signal that sync
    // end is not currently possible. However, we always support sync end
    // here unless the stream is already errored.
    if (errored) return -1;

    // If we're already closed, just return the total bytes written.
    if (closed) return totalBytesWritten;

    // Accepted writes and existing drain waiters must settle before ending.
    if (ending || pendingWrite !== null || drainWakeup !== null) return -1;

    // Fantastic, we can end synchronously!
    endWrite();
    closed = true;
    return totalBytesWritten;
  }

  function end(options = kEmptyObject) {
    const signal = getWriterSignal(options);
    return endAsync(signal);
  }

  async function endAsync(signal) {
    if (errored) throw error;
    if (closed) return totalBytesWritten;
    signal?.throwIfAborted();

    // Per the streams/iter spec, endSync and end follow a try-fallback
    // pattern. That is, callers should try endSync first and if it returns
    // -1, then they should call and await end(). This is a signal that sync
    // end is not currently possible. However, we always support sync end
    // here unless the stream is already errored.
    // While the user should have already called endSync, we call it again
    // here to actually process the end request. At worst it's called twice.
    const n = endSync();

    // A return value of -1 indicates that endSync was not yet able to
    // process the end request, either because we are errored or because we
    // are awaiting drain. If we're errored, throw the error. If we're waiting
    // for drain, await it and then try ending again.

    if (n >= 0) return n;
    if (errored) throw error;

    if (pendingEnd === null) {
      ending = true;
      pendingEnd = finishEnd();
    }
    return raceWithSignal(pendingEnd, signal);
  }

  async function finishEnd() {
    if (pendingWrite !== null) {
      await pendingWrite.done.promise;
    }
    if (errored) throw error;

    if (drainWakeup !== null) {
      await drainWakeup.promise;
    }
    if (errored) throw error;

    if (!writeEnded()) endWrite();
    closed = true;
    ending = false;
    return totalBytesWritten;
  }

  function fail(reason) {
    if (closed || errored) return;
    errored = true;
    error = reason;
    resetStream(error);
    if (drainWakeup != null) {
      markPromiseAsHandled(drainWakeup.promise);
      drainWakeup.reject(error);
      drainWakeup = null;
    }
  }

  const writer = {
    __proto__: null,
    get canWrite() {
      if (closed || ending || errored || writeEnded()) {
        return null;
      }
      return writeDesiredSize() > 0;
    },
    writeSync,
    write,
    writevSync,
    writev,
    endSync,
    end,
    fail,
    [drainableProtocol]() {
      if (closed || ending || errored) return null;
      // If a drain is already pending, return the existing promise.
      if (drainWakeup != null) return drainWakeup.promise;
      if (writeDesiredSize() > 0) return null;
      drainWakeup = PromiseWithResolvers();
      return drainWakeup.promise;
    },
    [SymbolAsyncDispose]() {
      if (ending) return pendingEnd;
      if (!closed && !errored) fail();
      return PromiseResolve();
    },
    [SymbolDispose]() {
      if (!closed && !errored) fail();
    },
  };

  // Non-writable stream - return a pre-closed writer.
  // A remote unidirectional stream is read-only and has no writable
  // side. isLocal distinguishes locally-initiated (writable) from
  // remotely-initiated (read-only) uni streams.
  if (nonWritableStream() || writeEnded()) {
    closed = true;
    return writer;
  }

  // Initialize the outbound DataQueue for streaming writes
  initStreamingSource();
  return writer;
}

// ============================================================================

module.exports = {
  getWriter,
};

/* c8 ignore stop */
