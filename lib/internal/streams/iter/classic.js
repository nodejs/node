'use strict';

// Interop utilities between classic Node.js streams and the stream/iter API.
//
// These are Node.js-specific (not part of the stream/iter spec) and are
// exported from 'stream/iter' as top-level utility functions:
//
//   fromReadable(readable)       -- classic Readable (or duck-type) -> stream/iter source
//   fromWritable(writable, opts) -- classic Writable (or duck-type) -> stream/iter Writer
//   toReadable(source, opts)     -- stream/iter source -> classic Readable
//   toReadableSync(source, opts) -- stream/iter source (sync) -> classic Readable
//   toWritable(writer)           -- stream/iter Writer -> classic Writable

const {
  ArrayIsArray,
  MathMax,
  NumberMAX_SAFE_INTEGER,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SafeWeakMap,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  AbortError,
  aggregateTwoErrors,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_STREAM_WRITE_AFTER_END,
  },
} = require('internal/errors');

const {
  validateInteger,
  validateObject,
} = require('internal/validators');

const { eos } = require('internal/streams/end-of-stream');
const {
  addAbortSignal: addAbortSignalNoValidate,
} = require('internal/streams/add-abort-signal');
const {
  queueMicrotask,
} = require('internal/process/task_queues');

const {
  toAsyncStreamable: kToAsyncStreamable,
  kValidatedSource,
  drainableProtocol,
} = require('internal/streams/iter/types');

const {
  validateBackpressure,
  toUint8Array,
} = require('internal/streams/iter/utils');

const { Buffer } = require('buffer');
const destroyImpl = require('internal/streams/destroy');

// Lazy-loaded to avoid circular dependencies. Readable and Writable
// both require this module's parent, so we defer the require.
let Readable;
let Writable;

function lazyReadable() {
  if (Readable === undefined) {
    Readable = require('internal/streams/readable');
  }
  return Readable;
}

function lazyWritable() {
  if (Writable === undefined) {
    Writable = require('internal/streams/writable');
  }
  return Writable;
}

// ============================================================================
// fromReadable(readable) -- classic Readable -> stream/iter async iterable
// ============================================================================

// Cache: one stream/iter source per Readable instance.
const fromReadableCache = new SafeWeakMap();

// Maximum chunks to drain into a single batch. Bounds peak memory when
// _read() synchronously pushes many chunks into the buffer.
const MAX_DRAIN_BATCH = 128;

const { normalizeAsyncValue } = require('internal/streams/iter/from');
const { isUint8Array } = require('internal/util/types');

// Normalize a batch of raw chunks from an object-mode or encoded
// Readable into Uint8Array values. Returns the normalized batch,
// or null if normalization produced no output.
async function normalizeBatch(raw) {
  const batch = [];
  for (let i = 0; i < raw.length; i++) {
    const value = raw[i];
    if (isUint8Array(value)) {
      batch.push(value);
    } else {
      // normalizeAsyncValue may await for async protocols (e.g.
      // toAsyncStreamable on yielded objects). Stream events during
      // the suspension are queued, not lost -- errors will surface
      // on the next loop iteration after this yield completes.
      for await (const normalized of normalizeAsyncValue(value)) {
        batch.push(normalized);
      }
    }
  }
  return batch.length > 0 ? batch : null;
}

// Batched async iterator for Readable streams. Same mechanism as
// createAsyncIterator (same event setup, same stream.read() to
// trigger _read(), same teardown) but drains all currently buffered
// chunks into a single Uint8Array[] batch per yield, amortizing the
// Promise/microtask cost across multiple chunks.
//
// When normalize is provided (object-mode / encoded streams), each
// drained batch is passed through it to convert chunks to Uint8Array.
// When normalize is null (byte-mode), chunks are already Buffers
// (Uint8Array subclass) and are yielded directly.
const nop = () => {};

async function* createBatchedAsyncIterator(stream, normalize) {
  let callback = nop;

  function next(resolve) {
    if (this === stream) {
      callback();
      callback = nop;
    } else {
      callback = resolve;
    }
  }

  stream.on('readable', next);

  let error;
  const cleanup = eos(stream, { writable: false }, (err) => {
    error = err ? aggregateTwoErrors(error, err) : null;
    callback();
    callback = nop;
  });

  try {
    while (true) {
      const chunk = stream.destroyed ? null : stream.read();
      if (chunk !== null) {
        const batch = [chunk];
        while (batch.length < MAX_DRAIN_BATCH &&
               stream._readableState?.length > 0) {
          const c = stream.read();
          if (c === null) break;
          batch.push(c);
        }
        if (normalize !== null) {
          const result = await normalize(batch);
          if (result !== null) {
            yield result;
          }
        } else {
          yield batch;
        }
      } else if (error) {
        throw error;
      } else if (error === null) {
        return;
      } else {
        await new Promise(next);
      }
    }
  } catch (err) {
    error = aggregateTwoErrors(error, err);
    throw error;
  } finally {
    if (error === undefined ||
        (stream._readableState?.autoDestroy)) {
      destroyImpl.destroyer(stream, null);
    } else {
      stream.off('readable', next);
      cleanup();
    }
  }
}

/**
 * Convert a classic Readable (or duck-type) to a stream/iter async iterable.
 *
 * If the object implements the toAsyncStreamable protocol, delegates to it.
 * Otherwise, duck-type checks for read() + EventEmitter (on/off) and
 * wraps with a batched async iterator.
 * @param {object} readable - A classic Readable or duck-type with
 *   read() and on()/off() methods.
 * @returns {AsyncIterable<Uint8Array[]>} A stream/iter async iterable source.
 */
function fromReadable(readable) {
  if (readable == null || typeof readable !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('readable', 'Readable', readable);
  }

  // Check cache first.
  const cached = fromReadableCache.get(readable);
  if (cached !== undefined) return cached;

  // Protocol path: object implements toAsyncStreamable.
  if (typeof readable[kToAsyncStreamable] === 'function') {
    const result = readable[kToAsyncStreamable]();
    fromReadableCache.set(readable, result);
    return result;
  }

  // Duck-type path: object has read() and EventEmitter methods.
  if (typeof readable.read !== 'function' ||
      typeof readable.on !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('readable', 'Readable', readable);
  }

  // Determine normalization. If the stream has _readableState, use it
  // to detect object-mode / encoding. Otherwise assume byte-mode.
  const state = readable._readableState;
  const normalize = (state && (state.objectMode || state.encoding)) ?
    normalizeBatch : null;

  const iter = createBatchedAsyncIterator(readable, normalize);
  iter[kValidatedSource] = true;
  iter.stream = readable;

  fromReadableCache.set(readable, iter);
  return iter;
}


// ============================================================================
// toReadable(source, options) -- stream/iter source -> classic Readable
// ============================================================================

const kNullPrototype = { __proto__: null };

/**
 * Create a byte-mode Readable from an AsyncIterable<Uint8Array[]>.
 * The source must yield Uint8Array[] batches (the stream/iter native
 * format). Each Uint8Array in a batch is pushed as a separate chunk.
 * @param {AsyncIterable<Uint8Array[]>} source
 * @param {object} [options]
 * @param {number} [options.highWaterMark]
 * @param {AbortSignal} [options.signal]
 * @returns {stream.Readable}
 */
function toReadable(source, options = kNullPrototype) {
  if (typeof source?.[SymbolAsyncIterator] !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('source', 'AsyncIterable', source);
  }

  validateObject(options, 'options');
  const {
    highWaterMark = 64 * 1024,
    signal,
  } = options;
  validateInteger(highWaterMark, 'options.highWaterMark', 0);

  const ReadableCtor = lazyReadable();
  const iterator = source[SymbolAsyncIterator]();
  let backpressure;
  let pumping = false;
  let done = false;

  const readable = new ReadableCtor({
    __proto__: null,
    highWaterMark,
    read() {
      if (backpressure) {
        const { resolve } = backpressure;
        backpressure = null;
        resolve();
      } else if (!pumping && !done) {
        pumping = true;
        pump();
      }
    },
    destroy(err, cb) {
      done = true;
      // Wake up the pump if it's waiting on backpressure so it
      // can see done === true and exit cleanly.
      if (backpressure) {
        backpressure.resolve();
        backpressure = null;
      }
      if (typeof iterator.return === 'function') {
        PromisePrototypeThen(iterator.return(),
                             () => cb(err), (e) => cb(e || err));
      } else {
        cb(err);
      }
    },
  });

  if (signal) {
    addAbortSignalNoValidate(signal, readable);
  }

  async function pump() {
    try {
      while (!done) {
        const { value: batch, done: iterDone } = await iterator.next();
        if (iterDone) {
          done = true;
          readable.push(null);
          return;
        }
        for (let i = 0; i < batch.length; i++) {
          if (!readable.push(batch[i])) {
            backpressure = PromiseWithResolvers();
            await backpressure.promise;
            if (done) return;
          }
        }
      }
    } catch (err) {
      done = true;
      readable.destroy(err);
    }
  }

  return readable;
}


// ============================================================================
// toReadableSync(source, options) -- stream/iter source (sync) -> Readable
// ============================================================================

/**
 * Create a byte-mode Readable from an Iterable<Uint8Array[]>.
 * Fully synchronous -- _read() pulls from the iterator directly.
 * @param {Iterable<Uint8Array[]>} source
 * @param {object} [options]
 * @param {number} [options.highWaterMark]
 * @returns {stream.Readable}
 */
function toReadableSync(source, options = kNullPrototype) {
  if (typeof source?.[SymbolIterator] !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('source', 'Iterable', source);
  }

  validateObject(options, 'options');
  const {
    highWaterMark = 64 * 1024,
  } = options;
  validateInteger(highWaterMark, 'options.highWaterMark', 0);

  const ReadableCtor = lazyReadable();
  const iterator = source[SymbolIterator]();

  return new ReadableCtor({
    __proto__: null,
    highWaterMark,
    read() {
      for (;;) {
        const { value: batch, done } = iterator.next();
        if (done) {
          this.push(null);
          return;
        }
        for (let i = 0; i < batch.length; i++) {
          if (!this.push(batch[i])) return;
        }
      }
    },
    destroy(err, cb) {
      if (typeof iterator.return === 'function') iterator.return();
      cb(err);
    },
  });
}


// ============================================================================
// fromWritable(writable, options) -- classic Writable -> stream/iter Writer
// ============================================================================

// Cache: one Writer adapter per Writable instance.
const fromWritableCache = new SafeWeakMap();

/**
 * Create a stream/iter Writer adapter from a classic Writable (or duck-type).
 *
 * Duck-type requirements: write() and on()/off() methods.
 * Falls back to sensible defaults for missing properties like
 * writableHighWaterMark, writableLength, writableObjectMode.
 * @param {object} writable - A classic Writable or duck-type.
 * @param {object} [options]
 * @param {string} [options.backpressure] - 'strict', 'block',
 *   'drop-newest'. 'drop-oldest' is not supported.
 * @returns {object} A stream/iter Writer adapter.
 */
function fromWritable(writable, options = kNullPrototype) {
  if (writable == null ||
      typeof writable.write !== 'function' ||
      typeof writable.on !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('writable', 'Writable', writable);
  }

  // Return cached adapter if available.
  const cached = fromWritableCache.get(writable);
  if (cached !== undefined) return cached;

  validateObject(options, 'options');
  const {
    backpressure = 'strict',
  } = options;
  validateBackpressure(backpressure);

  // The Writer interface is bytes-only. Object-mode Writables expect
  // arbitrary JS values, which is incompatible.
  if (writable.writableObjectMode) {
    throw new ERR_INVALID_STATE(
      'Cannot create a stream/iter Writer from an object-mode Writable');
  }

  // drop-oldest is not supported for classic stream.Writable. The
  // Writable's internal buffer stores individual { chunk, encoding,
  // callback } entries with no concept of batch boundaries. A writev()
  // call fans out into N separate buffer entries, so a subsequent
  // drop-oldest eviction could partially tear apart an earlier atomic
  // writev batch. The PushWriter avoids this because writev occupies a
  // single slot. Supporting drop-oldest here would require either
  // accepting partial writev eviction or adding batch tracking to the
  // buffer -- neither is acceptable without a deeper rework of Writable
  // internals.
  if (backpressure === 'drop-oldest') {
    throw new ERR_INVALID_ARG_VALUE('options.backpressure', backpressure,
                                    'drop-oldest is not supported for classic stream.Writable');
  }

  // Fall back to sensible defaults for duck-typed streams that may not
  // expose the full stream.Writable property set.
  const hwm = writable.writableHighWaterMark ?? 16384;
  let totalBytes = 0;

  // Waiters pending on backpressure resolution (block policy only).
  // Multiple un-awaited writes can each add a waiter, so this must be
  // a list. A single persistent 'drain' listener and 'error' listener
  // (installed once lazily) resolve or reject all waiters to avoid
  // accumulating per-write listeners on the stream.
  let waiters = [];
  let listenersInstalled = false;
  let onDrain;
  let onError;

  function installListeners() {
    if (listenersInstalled) return;
    listenersInstalled = true;
    onDrain = () => {
      const pending = waiters;
      waiters = [];
      for (let i = 0; i < pending.length; i++) {
        pending[i].resolve();
      }
    };
    onError = (err) => {
      const pending = waiters;
      waiters = [];
      for (let i = 0; i < pending.length; i++) {
        pending[i].reject(err);
      }
    };
    writable.on('drain', onDrain);
    writable.on('error', onError);
  }

  // Reject all pending waiters and remove the drain/error listeners.
  function cleanup(err) {
    const pending = waiters;
    waiters = [];
    for (let i = 0; i < pending.length; i++) {
      pending[i].reject(err ?? new AbortError());
    }
    if (!listenersInstalled) return;
    listenersInstalled = false;
    writable.removeListener('drain', onDrain);
    writable.removeListener('error', onError);
  }

  function waitForDrain() {
    const { promise, resolve, reject } = PromiseWithResolvers();
    waiters.push({ __proto__: null, resolve, reject });
    installListeners();
    return promise;
  }

  function isWritable() {
    // Duck-typed streams may not have these properties -- treat missing
    // as false (i.e., writable is still open).
    return !(writable.destroyed ?? false) &&
           !(writable.writableFinished ?? false) &&
           !(writable.writableEnded ?? false);
  }

  function isFull() {
    return (writable.writableLength ?? 0) >= hwm;
  }

  const writer = {
    __proto__: null,

    get desiredSize() {
      if (!isWritable()) return null;
      return MathMax(0, hwm - (writable.writableLength ?? 0));
    },

    writeSync(chunk) {
      return false;
    },

    writevSync(chunks) {
      return false;
    },

    // Backpressure semantics: write() resolves when the data is accepted
    // into the Writable's internal buffer, NOT when _write() has flushed
    // it to the underlying resource. This matches the Writer spec -- the
    // PushWriter resolves on buffer acceptance too. Classic Writable flow
    // control works the same way: write rapidly until write() returns
    // false, then wait for 'drain'. The _write callback is involved in
    // backpressure indirectly -- 'drain' fires after callbacks drain the
    // buffer below highWaterMark. Per-write errors from _write surface
    // as 'error' events caught by our generic error handler, rejecting
    // the next pending operation rather than the already-resolved one.
    //
    // The options.signal parameter from the Writer interface is ignored.
    // Classic stream.Writable has no per-write abort signal support;
    // cancellation should be handled at the pipeline level instead.
    write(chunk) {
      if (!isWritable()) {
        return PromiseReject(new ERR_STREAM_WRITE_AFTER_END());
      }

      let bytes;
      try {
        bytes = toUint8Array(chunk);
      } catch (err) {
        return PromiseReject(err);
      }

      if (backpressure === 'strict' && isFull()) {
        return PromiseReject(new ERR_INVALID_STATE.RangeError(
          'Backpressure violation: buffer is full. ' +
          'Await each write() call to respect backpressure.'));
      }

      if (backpressure === 'drop-newest' && isFull()) {
        // Silently discard. Still count bytes for consistency with
        // PushWriter, which counts dropped bytes in totalBytes.
        totalBytes += TypedArrayPrototypeGetByteLength(bytes);
        return PromiseResolve();
      }

      totalBytes += TypedArrayPrototypeGetByteLength(bytes);
      const ok = writable.write(bytes);
      if (ok) return PromiseResolve();

      // backpressure === 'block' (or strict with room that filled on
      // this write -- writable.write() accepted the data but returned
      // false indicating the buffer is now at/over hwm).
      if (backpressure === 'block') {
        return waitForDrain();
      }

      // strict: the write was accepted (there was room before writing)
      // but the buffer is now full. Resolve -- the *next* write will
      // be rejected if the caller ignores backpressure.
      return PromiseResolve();
    },

    writev(chunks) {
      if (!ArrayIsArray(chunks)) {
        throw new ERR_INVALID_ARG_TYPE('chunks', 'Array', chunks);
      }
      if (!isWritable()) {
        return PromiseReject(new ERR_STREAM_WRITE_AFTER_END());
      }

      if (backpressure === 'strict' && isFull()) {
        return PromiseReject(new ERR_INVALID_STATE.RangeError(
          'Backpressure violation: buffer is full. ' +
          'Await each write() call to respect backpressure.'));
      }

      if (backpressure === 'drop-newest' && isFull()) {
        // Discard entire batch.
        for (let i = 0; i < chunks.length; i++) {
          totalBytes +=
            TypedArrayPrototypeGetByteLength(toUint8Array(chunks[i]));
        }
        return PromiseResolve();
      }

      if (typeof writable.cork === 'function') writable.cork();
      let ok = true;
      for (let i = 0; i < chunks.length; i++) {
        const bytes = toUint8Array(chunks[i]);
        totalBytes += TypedArrayPrototypeGetByteLength(bytes);
        ok = writable.write(bytes);
      }
      if (typeof writable.uncork === 'function') writable.uncork();

      if (ok) return PromiseResolve();

      if (backpressure === 'block') {
        return waitForDrain();
      }

      return PromiseResolve();
    },

    endSync() {
      return -1;
    },

    // options.signal is ignored for the same reason as write().
    end() {
      if ((writable.writableFinished ?? false) ||
          (writable.destroyed ?? false)) {
        cleanup();
        return PromiseResolve(totalBytes);
      }

      const { promise, resolve, reject } = PromiseWithResolvers();

      if (!(writable.writableEnded ?? false)) {
        writable.end();
      }

      eos(writable, { writable: true, readable: false }, (err) => {
        cleanup(err);
        if (err) reject(err);
        else resolve(totalBytes);
      });

      return promise;
    },

    fail(reason) {
      cleanup(reason);
      if (typeof writable.destroy === 'function') {
        writable.destroy(reason);
      }
    },

    [SymbolAsyncDispose]() {
      if (isWritable()) {
        cleanup();
        if (typeof writable.destroy === 'function') {
          writable.destroy();
        }
      }
      return PromiseResolve();
    },

    [SymbolDispose]() {
      if (isWritable()) {
        cleanup();
        if (typeof writable.destroy === 'function') {
          writable.destroy();
        }
      }
    },
  };

  // drainableProtocol
  writer[drainableProtocol] = function() {
    if (!isWritable()) return null;
    if ((writable.writableLength ?? 0) < hwm) {
      return PromiseResolve(true);
    }
    const { promise, resolve } = PromiseWithResolvers();
    waiters.push({
      __proto__: null,
      resolve() { resolve(true); },
      reject() { resolve(false); },
    });
    installListeners();
    return promise;
  };

  fromWritableCache.set(writable, writer);
  return writer;
}


// ============================================================================
// toWritable(writer) -- stream/iter Writer -> classic Writable
// ============================================================================

/**
 * Create a classic stream.Writable backed by a stream/iter Writer.
 * Each _write/_writev call delegates to the Writer's methods,
 * attempting the sync path first (writeSync/writevSync/endSync) and
 * falling back to async if the sync path returns false or throws.
 * @param {object} writer - A stream/iter Writer (only write() is required).
 * @returns {stream.Writable}
 */
function toWritable(writer) {
  if (typeof writer?.write !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('writer', 'Writer', writer);
  }

  const WritableCtor = lazyWritable();

  const hasWriteSync = typeof writer.writeSync === 'function';
  const hasWritev = typeof writer.writev === 'function';
  const hasWritevSync = hasWritev &&
                        typeof writer.writevSync === 'function';
  const hasEnd = typeof writer.end === 'function';
  const hasEndSync = hasEnd &&
                     typeof writer.endSync === 'function';
  const hasFail = typeof writer.fail === 'function';

  // Try-sync-first pattern: attempt the synchronous method and
  // fall back to the async method if it returns false (indicating
  // the sync path was not accepted) or throws. When the sync path
  // succeeds, the callback is deferred via queueMicrotask to
  // preserve the async resolution contract that Writable internals
  // expect from _write/_writev/_final callbacks.

  function _write(chunk, encoding, cb) {
    const bytes = typeof chunk === 'string' ?
      Buffer.from(chunk, encoding) : chunk;
    if (hasWriteSync) {
      try {
        if (writer.writeSync(bytes)) {
          queueMicrotask(cb);
          return;
        }
      } catch {
        // Sync path threw -- fall through to async.
      }
    }
    try {
      PromisePrototypeThen(writer.write(bytes), () => cb(), cb);
    } catch (err) {
      cb(err);
    }
  }

  function _writev(entries, cb) {
    const chunks = [];
    for (let i = 0; i < entries.length; i++) {
      const { chunk, encoding } = entries[i];
      chunks[i] = typeof chunk === 'string' ?
        Buffer.from(chunk, encoding) : chunk;
    }
    if (hasWritevSync) {
      try {
        if (writer.writevSync(chunks)) {
          queueMicrotask(cb);
          return;
        }
      } catch {
        // Sync path threw -- fall through to async.
      }
    }
    try {
      PromisePrototypeThen(writer.writev(chunks), () => cb(), cb);
    } catch (err) {
      cb(err);
    }
  }

  function _final(cb) {
    if (!hasEnd) {
      queueMicrotask(cb);
      return;
    }
    if (hasEndSync) {
      try {
        const result = writer.endSync();
        if (result >= 0) {
          queueMicrotask(cb);
          return;
        }
      } catch {
        // Sync path threw -- fall through to async.
      }
    }
    try {
      PromisePrototypeThen(writer.end(), () => cb(), cb);
    } catch (err) {
      cb(err);
    }
  }

  function _destroy(err, cb) {
    if (err && hasFail) {
      writer.fail(err);
    }
    cb();
  }

  const writableOptions = {
    __proto__: null,
    // Use MAX_SAFE_INTEGER to effectively disable the Writable's
    // internal buffering. The underlying stream/iter Writer has its
    // own backpressure handling; we want _write to be called
    // immediately so the Writer can manage flow control directly.
    highWaterMark: NumberMAX_SAFE_INTEGER,
    write: _write,
    final: _final,
    destroy: _destroy,
  };

  if (hasWritev) {
    writableOptions.writev = _writev;
  }

  return new WritableCtor(writableOptions);
}


module.exports = {
  // Shared helpers used by Readable.prototype[toAsyncStreamable] in
  // readable.js to avoid duplicating the batched iterator logic.
  createBatchedAsyncIterator,
  normalizeBatch,

  // Public utilities exported from 'stream/iter'.
  fromReadable,
  fromWritable,
  toReadable,
  toReadableSync,
  toWritable,
};
