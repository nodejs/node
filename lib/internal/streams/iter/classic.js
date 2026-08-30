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
  ArrayPrototypePush,
  FunctionPrototypeCall,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SafeMap,
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
    ERR_FALSY_VALUE_REJECTION,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_OPERATION_FAILED,
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
  convertChunks,
  getWriterSignal,
  onSignalAbort,
  validateBackpressure,
  toWriterUint8Array,
} = require('internal/streams/iter/utils');

const { Buffer } = require('buffer');
const destroyImpl = require('internal/streams/destroy');
const { isError } = require('internal/util');
const { RingBuffer } = require('internal/streams/iter/ringbuffer');

// Classic stream error channels require a truthy Error object.
function toClassicError(reason, reasonMap) {
  try {
    if (isError(reason)) return reason;
  } catch {
    // Wrap values whose proxy traps make the Error check fail.
  }
  let error;
  if (!reason) {
    error = new ERR_FALSY_VALUE_REJECTION.HideStackFramesError(reason);
  } else {
    error = new ERR_OPERATION_FAILED('Non-Error value');
    error.reason = reason;
  }
  reasonMap?.set(error, { __proto__: null, reason });
  return error;
}

function raceWithSignal(promise, signal) {
  if (signal === undefined) return promise;
  if (signal.aborted) return PromiseReject(signal.reason);

  const {
    promise: signaledPromise,
    resolve,
    reject,
  } = PromiseWithResolvers();
  const onAbort = () => reject(signal.reason);
  signal.addEventListener('abort', onAbort, {
    __proto__: null,
    once: true,
  });
  PromisePrototypeThen(
    promise,
    (value) => {
      signal.removeEventListener('abort', onAbort);
      resolve(value);
    },
    (reason) => {
      signal.removeEventListener('abort', onAbort);
      reject(reason);
    },
  );
  return signaledPromise;
}

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
      ArrayPrototypePush(batch, value);
    } else {
      // normalizeAsyncValue may await for async protocols (e.g.
      // toAsyncStreamable on yielded objects). Stream events during
      // the suspension are queued, not lost -- errors will surface
      // on the next loop iteration after this yield completes.
      for await (const normalized of normalizeAsyncValue(value)) {
        ArrayPrototypePush(batch, normalized);
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

function createBatchedAsyncIterator(stream, normalize) {
  let callback = nop;
  let canceled = false;
  let started = false;

  function next(resolve) {
    if (this === stream) {
      callback();
      callback = nop;
    } else {
      callback = resolve;
    }
  }

  function wake() {
    callback();
    callback = nop;
  }

  async function* generate() {
    stream.on('readable', next);

    let error;
    const cleanup = eos(stream, { writable: false }, (err) => {
      error = err ? aggregateTwoErrors(error, err) : null;
      stream.removeListener('readable', next);
      wake();
    });
    const cleanupAll = () => {
      stream.removeListener('close', cleanupAll);
      cleanup();
    };
    stream.on('close', cleanupAll);

    try {
      while (!canceled) {
        const chunk = stream.destroyed ? null : stream.read();
        if (chunk !== null) {
          const batch = [chunk];
          while (batch.length < MAX_DRAIN_BATCH &&
                 stream._readableState?.length > 0) {
            const c = stream.read();
            if (c === null) break;
            ArrayPrototypePush(batch, c);
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
      stream.removeListener('readable', next);
      if (error === undefined ||
          (stream._readableState?.autoDestroy)) {
        destroyImpl.destroyer(stream, null);
        if (stream._readableState?.closeEmitted) cleanupAll();
      } else {
        cleanupAll();
      }
    }
  }

  const iterator = generate();
  const iteratorNext = iterator.next;
  const iteratorReturn = iterator.return;
  const iteratorThrow = iterator.throw;

  iterator.next = function(value) {
    started = true;
    return FunctionPrototypeCall(iteratorNext, iterator, value);
  };
  iterator.return = function(value) {
    canceled = true;
    wake();
    if (!started) stream.destroy();
    return FunctionPrototypeCall(iteratorReturn, iterator, value);
  };
  iterator.throw = function(reason) {
    canceled = true;
    wake();
    if (!started) stream.destroy();
    return FunctionPrototypeCall(iteratorThrow, iterator, reason);
  };

  return iterator;
}

/**
 * Convert a classic Readable (or duck-type) to a stream/iter async iterable.
 *
 * If the object implements the toAsyncStreamable protocol, delegates to it.
 * Otherwise, duck-type checks for read(), pipe(), destroy(), and EventEmitter
 * on()/removeListener() methods and
 * wraps with a batched async iterator.
 * @param {object} readable - A classic Readable or duck-type with
 *   read(), pipe(), destroy(), on(), and removeListener() methods.
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
      typeof readable.pipe !== 'function' ||
      typeof readable.destroy !== 'function' ||
      typeof readable.on !== 'function' ||
      typeof readable.removeListener !== 'function') {
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
      try {
        const returnMethod = iterator.return;
        if (typeof returnMethod !== 'function') {
          cb(err);
          return;
        }
        const returned = FunctionPrototypeCall(returnMethod, iterator);
        PromisePrototypeThen(PromiseResolve(returned), () => cb(err),
                             (error) => cb(err || toClassicError(error)));
      } catch (error) {
        cb(err || toClassicError(error));
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
      readable.destroy(toClassicError(err));
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
  let hasBatch = false;
  let batch;
  let batchIndex = 0;

  return new ReadableCtor({
    __proto__: null,
    highWaterMark,
    read() {
      try {
        for (;;) {
          if (hasBatch) {
            while (batchIndex < batch.length) {
              if (!this.push(batch[batchIndex++])) return;
            }
            batch = undefined;
            hasBatch = false;
            batchIndex = 0;
          }

          const result = iterator.next();
          const { done } = result;
          if (done) {
            this.push(null);
            return;
          }
          batch = result.value;
          hasBatch = true;
        }
      } catch (error) {
        const classicError = toClassicError(error);
        throw classicError;
      }
    },
    destroy(err, cb) {
      batch = undefined;
      hasBatch = false;
      try {
        const returnMethod = iterator.return;
        if (typeof returnMethod === 'function') {
          FunctionPrototypeCall(returnMethod, iterator);
        }
      } catch (error) {
        cb(err || toClassicError(error));
        return;
      }
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
 * Duck-type requirements: write(), end(), destroy(), on(), and
 * removeListener() methods.
 * Falls back to sensible defaults for missing properties like
 * writableHighWaterMark, writableLength, writableObjectMode.
 * @param {object} writable - A classic Writable or duck-type.
 * @param {object} [options]
 * @param {string} [options.backpressure] - 'strict', 'unbounded',
 *   'drop-newest'. 'drop-oldest' is not supported.
 * @returns {object} A stream/iter Writer adapter.
 */
function fromWritable(writable, options = kNullPrototype) {
  if (writable == null ||
      typeof writable.write !== 'function' ||
      typeof writable.end !== 'function' ||
      typeof writable.destroy !== 'function' ||
      typeof writable.on !== 'function' ||
      typeof writable.removeListener !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('writable', 'Writable', writable);
  }

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

  // Return cached adapter if available. Backpressure policy changes writer
  // behavior, so cache one adapter per policy.
  let cachedByBackpressure = fromWritableCache.get(writable);
  if (cachedByBackpressure !== undefined) {
    const cached = cachedByBackpressure.get(backpressure);
    if (cached !== undefined) return cached;
  } else {
    cachedByBackpressure = new SafeMap();
    fromWritableCache.set(writable, cachedByBackpressure);
  }

  // Fall back to sensible defaults for duck-typed streams that may not
  // expose the full stream.Writable property set.
  const hwm = writable.writableHighWaterMark ?? 16384;
  let totalBytes = 0;
  let errored = false;
  let error;
  let ending = false;
  let endStarted = false;
  let pendingEnd;
  let needsDrain = false;
  let finished = false;
  let pendingWrites = new RingBuffer();
  let drainWaiters = [];
  let drainListenerInstalled = false;
  let terminalListenersInstalled = false;

  function installDrainListener() {
    if (drainListenerInstalled) return;
    drainListenerInstalled = true;
    writable.on('drain', onDrain);
  }

  function removeDrainListenerIfIdle() {
    if (!drainListenerInstalled ||
        pendingWrites.length !== 0 ||
        drainWaiters.length !== 0) {
      return;
    }
    drainListenerInstalled = false;
    writable.removeListener('drain', onDrain);
  }

  function removeTerminalListeners() {
    if (!terminalListenersInstalled) return;
    terminalListenersInstalled = false;
    writable.removeListener('error', onError);
    writable.removeListener('finish', onFinish);
    writable.removeListener('close', onClose);
  }

  function cleanupPendingSignal(entry) {
    if (entry.signal === undefined) return;
    entry.signal.removeEventListener('abort', entry.onAbort);
    entry.signal = undefined;
    entry.onAbort = undefined;
  }

  function cleanup(
    reason,
    preserveReason = false,
    keepTerminalListeners = false,
  ) {
    const pending = drainWaiters;
    drainWaiters = [];
    for (let i = 0; i < pending.length; i++) {
      if (!preserveReason &&
          (reason === undefined || reason === null) &&
          pending[i].close !== undefined) {
        pending[i].close();
      } else {
        pending[i].reject(
          preserveReason ? reason : reason ?? new AbortError());
      }
    }

    const writes = pendingWrites;
    pendingWrites = new RingBuffer();
    while (writes.length !== 0) {
      const entry = writes.shift();
      cleanupPendingSignal(entry);
      entry.reject(
        preserveReason ? reason : reason ?? new AbortError());
    }

    if (drainListenerInstalled) {
      drainListenerInstalled = false;
      writable.removeListener('drain', onDrain);
    }
    if (!keepTerminalListeners) removeTerminalListeners();
  }

  function isUnderlyingWritable() {
    syncWritableError();
    // Duck-typed streams may not have these properties -- treat missing
    // as false (i.e., writable is still open).
    return !errored && !finished &&
           !(writable.destroyed ?? false) &&
           !(writable.writableFinished ?? false) &&
           !(writable.writableEnded ?? false);
  }

  function isWritable() {
    return !ending && isUnderlyingWritable();
  }

  function isFull() {
    return needsDrain ||
           (writable.writableNeedDrain ?? false) ||
           (hwm > 0 && (writable.writableLength ?? 0) >= hwm);
  }

  function writeChunks(chunks) {
    let ok = true;
    for (let i = 0; i < chunks.length; i++) {
      const bytes = chunks[i];
      if (!writable.write(bytes)) {
        needsDrain = true;
        ok = false;
      }
      totalBytes += TypedArrayPrototypeGetByteLength(bytes);
    }
    return ok;
  }

  function writeBatch(chunks) {
    if (typeof writable.cork !== 'function' ||
        typeof writable.uncork !== 'function') {
      return writeChunks(chunks);
    }
    writable.cork();
    try {
      return writeChunks(chunks);
    } finally {
      writable.uncork();
    }
  }

  function maybeStartEnd() {
    if (ending && pendingWrites.length === 0) startEnd();
  }

  function settleDrainWaiters(value) {
    const pending = drainWaiters;
    drainWaiters = [];
    for (let i = 0; i < pending.length; i++) {
      pending[i].resolve(value);
    }
  }

  function flushPendingWrites() {
    while (pendingWrites.length !== 0 &&
           isUnderlyingWritable() &&
           !isFull()) {
      const entry = pendingWrites.shift();
      cleanupPendingSignal(entry);
      let ok;
      try {
        ok = writeBatch(entry.chunks);
      } catch (reason) {
        entry.reject(reason);
        continue;
      }
      if (errored) {
        entry.reject(error);
        break;
      }
      entry.resolve();
      if (!ok) break;
    }

    if (pendingWrites.length !== 0) installDrainListener();
    if (pendingWrites.length === 0) {
      if (ending) {
        settleDrainWaiters(false);
      } else if (isUnderlyingWritable() && !isFull()) {
        settleDrainWaiters(true);
      }
    }
    removeDrainListenerIfIdle();
    maybeStartEnd();
  }

  function queueWrite(chunks, signal) {
    const { promise, resolve, reject } = PromiseWithResolvers();
    const entry = {
      __proto__: null,
      chunks,
      resolve,
      reject,
      signal: undefined,
      onAbort: undefined,
    };
    pendingWrites.push(entry);
    installDrainListener();

    if (signal !== undefined) {
      entry.signal = signal;
      entry.onAbort = () => {
        const index = pendingWrites.indexOf(entry);
        if (index === -1) return;
        pendingWrites.removeAt(index);
        cleanupPendingSignal(entry);
        reject(signal.reason);
        removeDrainListenerIfIdle();
        flushPendingWrites();
      };
      onSignalAbort(signal, entry.onAbort);
    }

    return promise;
  }

  function onDrain() {
    needsDrain = false;
    flushPendingWrites();
  }

  function finishWithError(reason, keepTerminalListeners = false) {
    if (errored) return;
    errored = true;
    error = reason;
    ending = false;
    const end = pendingEnd;
    pendingEnd = undefined;
    cleanup(reason, true, keepTerminalListeners);
    end?.reject(reason);
  }

  function syncWritableError() {
    if (errored) return;
    const reason = writable.errored;
    if (reason !== undefined && reason !== null) {
      finishWithError(reason, true);
    }
  }

  function onError(reason) {
    if (errored) {
      removeTerminalListeners();
      return;
    }
    finishWithError(reason);
  }

  function onFinish() {
    if (errored || finished) return;
    finished = true;
    ending = false;
    const end = pendingEnd;
    pendingEnd = undefined;
    cleanup();
    end?.resolve(totalBytes);
  }

  function onClose() {
    syncWritableError();
    if (errored) {
      removeTerminalListeners();
      return;
    }
    if (finished || (writable.writableFinished ?? false)) {
      onFinish();
      removeTerminalListeners();
      return;
    }

    finished = true;
    ending = false;
    const end = pendingEnd;
    pendingEnd = undefined;
    const reason = new AbortError();
    cleanup(reason, true);
    end?.reject(reason);
  }

  function startEnd() {
    if (endStarted || pendingEnd === undefined) return;
    endStarted = true;
    const end = pendingEnd;

    try {
      if (!(writable.writableEnded ?? false)) writable.end();
      syncWritableError();
      if (!errored && (writable.writableFinished ?? false)) onFinish();
    } catch (reason) {
      pendingEnd = undefined;
      ending = false;
      errored = true;
      error = reason;
      cleanup(reason, true, true);
      end.reject(reason);
      try {
        writable.destroy(toClassicError(reason));
      } catch {
        removeTerminalListeners();
      }
    }
  }

  writable.on('error', onError);
  writable.on('finish', onFinish);
  writable.on('close', onClose);
  terminalListenersInstalled = true;
  syncWritableError();
  if (!errored && (writable.writableFinished ?? false)) {
    onFinish();
  } else if (!errored && (writable.destroyed ?? false)) {
    onClose();
  }

  const writer = {
    __proto__: null,

    get canWrite() {
      if (!isWritable()) return null;
      return pendingWrites.length === 0 && !isFull();
    },

    writeSync(chunk) {
      toWriterUint8Array(chunk);
      return false;
    },

    writevSync(chunks) {
      convertChunks(chunks);
      return false;
    },

    // Writes made with available capacity resolve when accepted by the classic
    // Writable. Writes made while it is full stay in this adapter until drain,
    // so operation signals can cancel them before they are committed.
    write(chunk, options) {
      const bytes = toWriterUint8Array(chunk);
      const signal = getWriterSignal(options);
      syncWritableError();
      if (errored) return PromiseReject(error);
      if (!isWritable()) {
        return PromiseReject(new ERR_STREAM_WRITE_AFTER_END());
      }
      if (signal?.aborted) return PromiseReject(signal.reason);

      if (pendingWrites.length !== 0 || isFull()) {
        if (backpressure === 'drop-newest') {
          totalBytes += TypedArrayPrototypeGetByteLength(bytes);
          return PromiseResolve();
        }
        if (backpressure === 'strict' && pendingWrites.length !== 0) {
          return PromiseReject(new ERR_INVALID_STATE.RangeError(
            'Backpressure violation: too many pending writes. ' +
            'Await each write() call to respect backpressure.'));
        }
        return queueWrite([bytes], signal);
      }

      try {
        writeChunks([bytes]);
      } catch (reason) {
        return PromiseReject(reason);
      }
      if (errored) return PromiseReject(error);
      return PromiseResolve();
    },

    writev(chunks, options) {
      chunks = convertChunks(chunks);
      const signal = getWriterSignal(options);
      syncWritableError();
      if (errored) return PromiseReject(error);
      if (!isWritable()) {
        return PromiseReject(new ERR_STREAM_WRITE_AFTER_END());
      }
      if (signal?.aborted) return PromiseReject(signal.reason);
      if (chunks.length === 0) return PromiseResolve();

      if (pendingWrites.length !== 0 || isFull()) {
        if (backpressure === 'drop-newest') {
          for (let i = 0; i < chunks.length; i++) {
            totalBytes += TypedArrayPrototypeGetByteLength(chunks[i]);
          }
          return PromiseResolve();
        }
        if (backpressure === 'strict' && pendingWrites.length !== 0) {
          return PromiseReject(new ERR_INVALID_STATE.RangeError(
            'Backpressure violation: too many pending writes. ' +
            'Await each write() call to respect backpressure.'));
        }
        return queueWrite(chunks, signal);
      }

      try {
        writeBatch(chunks);
      } catch (reason) {
        return PromiseReject(reason);
      }

      if (errored) return PromiseReject(error);
      return PromiseResolve();
    },

    endSync() {
      return -1;
    },

    end(options) {
      const signal = getWriterSignal(options);
      syncWritableError();
      if (errored) return PromiseReject(error);
      if (signal?.aborted) return PromiseReject(signal.reason);
      if (pendingEnd) return raceWithSignal(pendingEnd.promise, signal);
      if ((writable.writableFinished ?? false) ||
          (writable.destroyed ?? false)) {
        cleanup();
        return raceWithSignal(PromiseResolve(totalBytes), signal);
      }

      pendingEnd = PromiseWithResolvers();
      const { promise } = pendingEnd;
      ending = true;
      maybeStartEnd();
      return raceWithSignal(promise, signal);
    },

    fail(reason) {
      syncWritableError();
      if (errored ||
          (writable.writableFinished ?? false) ||
          (writable.destroyed ?? false)) {
        return;
      }
      errored = true;
      error = reason;
      ending = false;
      pendingEnd?.reject(reason);
      pendingEnd = undefined;
      cleanup(reason, true, true);
      try {
        writable.destroy(toClassicError(reason));
      } catch (destroyError) {
        removeTerminalListeners();
        throw destroyError;
      }
    },

    [SymbolAsyncDispose]() {
      if (pendingEnd) return pendingEnd.promise;
      if (isWritable()) {
        this.fail();
      }
      return PromiseResolve();
    },

    [SymbolDispose]() {
      this.fail();
    },
  };

  // drainableProtocol
  writer[drainableProtocol] = function() {
    if (!isWritable()) return null;
    if (pendingWrites.length === 0 && !isFull()) {
      return PromiseResolve(true);
    }
    const { promise, resolve, reject } = PromiseWithResolvers();
    ArrayPrototypePush(drainWaiters, {
      __proto__: null,
      resolve,
      reject,
      close() { resolve(false); },
    });
    installDrainListener();
    return promise;
  };

  cachedByBackpressure.set(backpressure, writer);
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
  const hasDispose = typeof writer[SymbolDispose] === 'function';
  const hasAsyncDispose = typeof writer[SymbolAsyncDispose] === 'function';
  let writerEnded = false;
  const classicErrorReasons = new SafeWeakMap();
  // Try-sync-first pattern: attempt the synchronous method and fall back to the
  // async method if it returns false (data not accepted synchronously).
  // When the sync path succeeds, the callback is deferred via queueMicrotask
  // to preserve the async resolution contract that Writable internals expect
  // from _write/_writev/_final callbacks.

  function _write(chunk, encoding, cb) {
    const bytes = typeof chunk === 'string' ?
      Buffer.from(chunk, encoding) : chunk;
    if (hasWriteSync) {
      try {
        if (writer.writeSync(bytes)) {
          queueMicrotask(cb);
          return;
        }
        // WriteSync returned false: not accepted, fall through to async.
      } catch (err) {
        cb(toClassicError(err, classicErrorReasons));
        return;
      }
    }
    try {
      PromisePrototypeThen(
        writer.write(bytes), () => cb(),
        (err) => cb(toClassicError(err, classicErrorReasons)));
    } catch (err) {
      cb(toClassicError(err, classicErrorReasons));
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
        // WritevSync returned false: not accepted, fall through to async.
      } catch (err) {
        cb(toClassicError(err, classicErrorReasons));
        return;
      }
    }
    try {
      PromisePrototypeThen(
        writer.writev(chunks), () => cb(),
        (err) => cb(toClassicError(err, classicErrorReasons)));
    } catch (err) {
      cb(toClassicError(err, classicErrorReasons));
    }
  }

  function _final(cb) {
    if (!hasEnd) {
      writerEnded = true;
      queueMicrotask(cb);
      return;
    }
    if (hasEndSync) {
      try {
        const result = writer.endSync();
        if (result >= 0) {
          writerEnded = true;
          queueMicrotask(cb);
          return;
        }
        // Result < 0: can't end synchronously, fall through to async.
      } catch (err) {
        cb(toClassicError(err, classicErrorReasons));
        return;
      }
    }
    try {
      PromisePrototypeThen(
        writer.end(), () => {
          writerEnded = true;
          cb();
        },
        (err) => cb(toClassicError(err, classicErrorReasons)));
    } catch (err) {
      cb(toClassicError(err, classicErrorReasons));
    }
  }

  function _destroy(err, cb) {
    if (!err && writerEnded) {
      cb();
      return;
    }

    let result;
    try {
      if (hasFail) {
        if (err) {
          const wrapped = classicErrorReasons.get(err);
          classicErrorReasons.delete(err);
          writer.fail(wrapped === undefined ? err : wrapped.reason);
        } else {
          writer.fail();
        }
      } else if (hasDispose) {
        writer[SymbolDispose]();
      } else if (hasAsyncDispose) {
        result = writer[SymbolAsyncDispose]();
      }
    } catch (error) {
      cb(err || toClassicError(error, classicErrorReasons));
      return;
    }

    if (result !== undefined) {
      PromisePrototypeThen(
        PromiseResolve(result),
        () => cb(err),
        (error) => cb(err || toClassicError(error, classicErrorReasons)));
      return;
    }
    cb(err);
  }

  const writableOptions = {
    __proto__: null,
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
