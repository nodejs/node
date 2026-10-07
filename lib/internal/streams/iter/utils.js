'use strict';

const {
  Array,
  ArrayBufferPrototypeGetByteLength,
  ArrayPrototypeSlice,
  FunctionPrototypeCall,
  ObjectDefineProperty,
  ObjectFreeze,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SafeWeakSet,
  SymbolAsyncIterator,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  TypedArrayPrototypeSet,
  Uint8Array,
} = primordials;

const {
  markPromiseAsHandled,
} = internalBinding('util');

const { TextEncoder } = require('internal/encoding');
const { markAsUntransferable } = require('internal/buffer');
const { utf8WriteStatic } = internalBinding('buffer');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const { isSharedArrayBuffer, isUint8Array } = require('internal/util/types');
const { kWeakHandler } = require('internal/event_target');
const {
  AbortController,
  abortSignal,
} = require('internal/abort_controller');
const { RingBuffer } = require('internal/streams/iter/ringbuffer');

const {
  validateInteger,
  validateOneOf,
} = require('internal/validators');
const {
  converters,
} = require('internal/streams/iter/webidl');
const {
  kValidatedTransform,
} = require('internal/streams/iter/types');

// Shared `addEventListener()` options for one-time 'abort' listeners. Frozen
// because signals can come from user code, and a patched addEventListener()
// must not be able to change the options for every later registration.
const kNullOnceOption = ObjectFreeze({ __proto__: null, once: true });

// Cached resolved promise to avoid allocating a new one on every sync fast-path.
const kResolvedPromise = PromiseResolve();

// Shared TextEncoder instance for string conversion.
const encoder = new TextEncoder();

// Small strings are encoded into a pool, like Buffer.from(string), instead
// of each into an ArrayBuffer of its own: writing many small strings, as a
// server-side renderer does, is otherwise mostly the cost of allocating
// them. The pool is untransferable, so that transferring the buffer of one
// chunk cannot detach the others. A string is pooled when its UTF-8 encoding
// fits in kMaxPooledByteLength bytes for certain: every UTF-16 code unit
// encodes to at most 3 bytes.
const kStringPoolSize = 64 * 1024;
const kMaxPooledLength = 8 * 1024 / 3;
let stringPool = null;
let stringPoolBuffer;
let stringPoolOffset = 0;

function encodeString(string) {
  const length = string.length;
  if (length > kMaxPooledLength) return encoder.encode(string);
  const maxByteLength = length * 3;
  if (stringPool === null ||
      maxByteLength > kStringPoolSize - stringPoolOffset) {
    stringPool = new Uint8Array(kStringPoolSize);
    stringPoolBuffer = TypedArrayPrototypeGetBuffer(stringPool);
    markAsUntransferable(stringPoolBuffer);
    stringPoolOffset = 0;
  }
  const offset = stringPoolOffset;
  const written = utf8WriteStatic(stringPool, string, offset, maxByteLength);
  // Keep the chunks 8-byte aligned, as Buffer does.
  stringPoolOffset = (offset + written + 7) & ~7;
  return new Uint8Array(stringPoolBuffer, offset, written);
}

// Default high water marks for push and multi-consumer streams. These values
// are somewhat arbitrary but have been tested across various workloads and
// appear to yield the best overall throughput/latency balance.

/** Minimum and default byte budget for push streams (single-consumer). */
const kPushDefaultBudget = 16384;

/** Default byte budget for broadcast and share streams (multi-consumer). */
const kMultiConsumerDefaultBudget = 65536;

/**
 * Iterator result object (`{ done, value }`) for the iterators returned by
 * this module. A result is created for every chunk, so it must be cheap.
 *
 * `new IterResult(done, value)` literals are created in V8 dictionary
 * mode, which costs several times more than an ordinary object. Instances
 * of this constructor have fast properties, always with the same shape
 * (`done` before `value`), and still have no %Object.prototype% in their
 * prototype chain, so a polluted `Object.prototype.then` cannot turn a
 * result into a thenable when an async `next()` resolves with it. The
 * prototype is a single empty, frozen, null-prototype object (V8 gives
 * objects fast properties once they are used as a prototype).
 * @param {boolean} done
 * @param {any} value
 */
function IterResult(done, value) {
  this.done = done;
  this.value = value;
}
// The non-enumerable `constructor` lets util.inspect() print results as
// `IterResult { done, value }`.
IterResult.prototype = ObjectFreeze(ObjectDefineProperty(
  { __proto__: null }, 'constructor', { __proto__: null, value: IterResult }));

/**
 * Register a handler for an AbortSignal, handling the already-aborted case.
 * If the signal is already aborted, calls handler immediately.
 * Otherwise, adds a one-time 'abort' listener.
 * @param {AbortSignal} signal
 * @param {Function} handler
 */
function onSignalAbort(signal, handler) {
  if (signal.aborted) {
    handler();
  } else {
    signal.addEventListener('abort', handler, kNullOnceOption);
  }
}

/**
 * The abort state of a pipeline: what the AbortController of a pipeline in
 * the specification holds, without creating an AbortController unless its
 * signal is used. Most transforms never read the signal passed to them, so
 * most pipelines never need one. When the signal is first read, it is
 * created aborted with the same reason if the pipeline has been aborted:
 * nothing can have listened to it before, so this cannot be told apart from
 * a signal created with the pipeline. Code inside the pipeline checks
 * `aborted` instead of reading the signal.
 */
class PipelineAbort {
  aborted = false;
  reason = undefined;
  #controller = null;

  get signal() {
    let controller = this.#controller;
    if (controller === null) {
      controller = this.#controller = new AbortController();
      if (this.aborted) abortSignal(controller.signal, this.reason);
    }
    return controller.signal;
  }

  /**
   * Abort the pipeline with `reason`, if it is not aborted already. Abort
   * listeners on the signal, if it has been created, run synchronously, and
   * errors they throw propagate.
   * @param {any} reason
   */
  abort(reason) {
    if (this.aborted) return;
    this.aborted = true;
    this.reason = reason;
    if (this.#controller !== null) {
      abortSignal(this.#controller.signal, reason);
    }
  }

  throwIfAborted() {
    if (this.aborted) throw this.reason;
  }
}

/**
 * Wait for `consume(iterator)`, which reads `iterator` (a normalized source)
 * to the end, unless `signal` aborts first. On an abort, `state.aborted` is
 * set, the source is closed without waiting, ignoring errors, and the
 * returned promise rejects with the abort reason at once, as a pull()
 * pipeline with `signal` would reject its pending pull; `consume` must check
 * `state.aborted` after each read and stop. Closing and rejecting wait for a
 * microtask, as the signal can abort while the source is called.
 *
 * This takes one listener and one promise for the whole read, instead of a
 * layer around the source adding a promise to every read.
 * @param {AsyncIterator} iterator
 * @param {AbortSignal} signal
 * @param {{ aborted: boolean }} state
 * @param {Function} consume
 * @returns {Promise<any>}
 */
function raceSignal(iterator, signal, state, consume) {
  const { promise, resolve, reject } = PromiseWithResolvers();
  let settled = false;
  function closeAndReject() {
    try {
      const returnMethod = iterator.return;
      if (typeof returnMethod === 'function') {
        markPromiseAsHandled(PromiseResolve(
          FunctionPrototypeCall(returnMethod, iterator)));
      }
    } catch {
      // The abort takes precedence over errors closing the source.
    }
    reject(signal.reason);
  }

  function onAbort() {
    if (settled) return;
    settled = true;
    state.aborted = true;
    PromisePrototypeThen(kResolvedPromise, closeAndReject);
  }
  // Added first, as the signal can abort while the source is read.
  signal.addEventListener('abort', onAbort, kNullOnceOption);
  let consumed;
  try {
    consumed = consume(iterator);
  } catch (error) {
    consumed = PromiseReject(error);
  }
  PromisePrototypeThen(consumed, (value) => {
    if (settled) return;
    settled = true;
    signal.removeEventListener('abort', onAbort);
    resolve(value);
  }, (error) => {
    if (settled) return;
    settled = true;
    signal.removeEventListener('abort', onAbort);
    reject(error);
  });
  return promise;
}

/**
 * Wrap an async source so each pending read is abort-aware.
 * @param {AsyncIterable<Uint8Array[]>} source - The source to read from.
 * @param {AbortSignal|undefined} signal - Optional abort signal.
 * @returns {AsyncIterable<Uint8Array[]>}
 */
function yieldAbortable(source, signal) {
  if (signal === undefined) {
    return source;
  }

  return {
    __proto__: null,
    [SymbolAsyncIterator]() {
      return createAbortableIterator(source, signal);
    },
  };
}

/**
 * Iterator reading `source` until `signal` aborts, for yieldAbortable().
 *
 * This is what an async generator looping over the source with
 * abortableNext() would do, written out by hand: that costs an abort
 * listener, a promise race and an async generator layer for every value.
 * Instead, a single abort listener is added for the whole iteration, held
 * weakly so that the signal does not keep the iterator alive, and each
 * next() waits with a single promise that an abort rejects. As with the
 * generator:
 * - the source is opened by the first next(), unless the signal has aborted
 *   by then, and calls made while one is in progress are queued;
 * - an abort before or while reading a value, or once it has been read,
 *   rejects with the abort reason;
 * - unless the source has ended, an error (including an abort) closes it:
 *   if the signal is aborted, without waiting, otherwise waiting and
 *   rejecting with an error from closing it instead;
 * - return() closes the source, propagating errors from closing it.
 * @param {AsyncIterable} source
 * @param {AbortSignal} signal
 * @returns {AsyncIterator}
 */
function createAbortableIterator(source, signal) {
  let state = kStart;
  let iterator;
  let completed = false;
  // The settling functions of the pending next(), if any.
  let resolveNext = null;
  let rejectNext = null;
  const operations = createOperationQueue();

  const self = ObjectSetPrototypeOf({
    next() { return operations.run(doNext); },
    return(value) { return operations.run(doReturn, value); },
    throw(error) { return operations.run(doThrow, error); },
    [SymbolAsyncIterator]() { return this; },
  }, null);

  function onAbort() {
    if (rejectNext === null) return;
    const reject = rejectNext;
    resolveNext = rejectNext = null;
    fail(reject, signal.reason);
  }

  // What the generator did in its `catch` and `finally` blocks for `error`.
  function fail(reject, error) {
    state = kDone;
    signal.removeEventListener('abort', onAbort);
    const aborted = signal.aborted;
    if (!completed && typeof iterator.return === 'function') {
      let closed;
      try {
        const result = iterator.return();
        if (aborted) {
          // PromiseResolve(result) can reject if result is a thenable that
          // rejects, so mark it as handled even though the abort takes
          // precedence over the result of iterator.return().
          markPromiseAsHandled(PromiseResolve(result));
        } else {
          closed = PromiseResolve(result);
        }
      } catch (closeError) {
        operations.settled();
        reject(closeError);
        return;
      }
      if (closed !== undefined) {
        PromisePrototypeThen(closed, () => {
          operations.settled();
          reject(error);
        }, (closeError) => {
          operations.settled();
          reject(closeError);
        });
        return;
      }
    }
    operations.settled();
    reject(error);
  }

  function onResult(result) {
    if (resolveNext === null) return;  // Settled by an abort.
    const resolve = resolveNext;
    const reject = rejectNext;
    resolveNext = rejectNext = null;
    let done;
    let value;
    try {
      ({ done, value } = result);
      if (!done) signal.throwIfAborted();
    } catch (error) {
      fail(reject, error);
      return;
    }
    if (done) {
      completed = true;
      state = kDone;
      signal.removeEventListener('abort', onAbort);
      operations.settled();
      resolve(new IterResult(true, undefined));
      return;
    }
    operations.settled();
    resolve(new IterResult(false, value));
  }

  function onError(error) {
    if (rejectNext === null) return;  // Settled by an abort.
    const reject = rejectNext;
    resolveNext = rejectNext = null;
    fail(reject, error);
  }

  function doNext() {
    if (state === kDone) {
      operations.settled();
      return PromiseResolve(new IterResult(true, undefined));
    }
    if (state === kStart) {
      // An abort before the first read ends the iteration without opening
      // the source.
      if (signal.aborted) {
        state = kDone;
        operations.settled();
        return PromiseReject(signal.reason);
      }
      try {
        iterator = source[SymbolAsyncIterator]();
      } catch (error) {
        state = kDone;
        operations.settled();
        return PromiseReject(error);
      }
      state = kActive;
      signal.addEventListener('abort', onAbort,
                              { __proto__: null, [kWeakHandler]: self });
    }
    const { promise, resolve, reject } = PromiseWithResolvers();
    let next;
    try {
      signal.throwIfAborted();
      next = PromiseResolve(iterator.next());
    } catch (error) {
      fail(reject, error);
      return promise;
    }
    resolveNext = resolve;
    rejectNext = reject;
    PromisePrototypeThen(next, onResult, onError);
    // The signal can abort while iterator.next() runs.
    if (signal.aborted) onAbort();
    return promise;
  }

  function doReturn(value) {
    const result = new IterResult(true, value);
    if (state !== kActive) {
      state = kDone;
      operations.settled();
      return PromiseResolve(result);
    }
    state = kDone;
    signal.removeEventListener('abort', onAbort);
    if (typeof iterator.return !== 'function') {
      operations.settled();
      return PromiseResolve(result);
    }
    let closed;
    try {
      closed = PromiseResolve(iterator.return());
    } catch (error) {
      operations.settled();
      return PromiseReject(error);
    }
    return PromisePrototypeThen(closed, () => {
      operations.settled();
      return result;
    }, (error) => {
      operations.settled();
      throw error;
    });
  }

  function doThrow(error) {
    if (state !== kActive) {
      state = kDone;
      operations.settled();
      return PromiseReject(error);
    }
    const { promise, reject } = PromiseWithResolvers();
    fail(reject, error);
    return promise;
  }

  return self;
}

/**
 * Compute the minimum cursor across a set of consumers and count how many
 * consumers are at that cursor.
 * @param {Set} consumers - Set of objects with a `cursor` property
 * @param {number} fallback - Cursor to return when set is empty
 * @returns {{ minCursor: number, minCursorConsumers: number }}
 */
// The result of getMinCursor(), reused: it runs whenever the slowest
// consumer of a share or broadcast advances, which can be once per batch,
// and its callers read the result at once.
const minCursorResult = { __proto__: null, minCursor: 0, minCursorConsumers: 0 };

function getMinCursor(consumers, fallback) {
  let minCursor = fallback;
  let minCursorConsumers = 0;
  for (const consumer of consumers) {
    if (consumer.cursor < minCursor) {
      minCursor = consumer.cursor;
      minCursorConsumers = 1;
    } else if (consumer.cursor === minCursor) {
      minCursorConsumers++;
    }
  }
  minCursorResult.minCursor = minCursor;
  minCursorResult.minCursorConsumers = minCursorConsumers;
  return minCursorResult;
}

/**
 * Convert a chunk (string or Uint8Array) to Uint8Array.
 * Strings are UTF-8 encoded.
 * @param {Uint8Array|string} chunk
 * @returns {Uint8Array}
 */
function toUint8Array(chunk) {
  if (typeof chunk === 'string') {
    return encodeString(chunk);
  }
  if (!isUint8Array(chunk)) {
    throw new ERR_INVALID_ARG_TYPE('chunk', ['string', 'Uint8Array'], chunk);
  }
  return chunk;
}

// Byte views are accepted with their byteLength, and rejected when used if
// it has changed: if they were detached (byteLength 0), resized with a
// length-tracking view, or shrunk out of bounds. What was accounted for a
// view is then what is there. The byteLength is read from the view alone,
// without its buffer: reading the buffer of a small typed array that V8
// keeps on the heap moves its contents to a new ArrayBuffer.
//
// Byte views and batch entries are created for every chunk and every batch,
// so like IterResult they are constructed rather than created as
// `{ __proto__: null, ... }` literals (which are dictionary-mode objects),
// and their prototype is an empty null-prototype object.
function FixedByteView(value, byteLength) {
  this.value = value;
  this.byteLength = byteLength;
}
FixedByteView.prototype = ObjectFreeze({ __proto__: null });

function BatchEntry(views, byteLength) {
  this.views = views;
  this.byteLength = byteLength;
}
BatchEntry.prototype = ObjectFreeze({ __proto__: null });

// Waiters for reads, writes and drains are queued whenever a stream has
// to wait, which can be once per chunk.
function PendingRequest(resolve, reject) {
  this.resolve = resolve;
  this.reject = reject;
}
PendingRequest.prototype = ObjectFreeze({ __proto__: null });

function PendingWrite(batch, resolve, reject) {
  this.batch = batch;
  this.resolve = resolve;
  this.reject = reject;
}
PendingWrite.prototype = ObjectFreeze({ __proto__: null });

function snapshotByteView(value) {
  return new FixedByteView(value, TypedArrayPrototypeGetByteLength(value));
}

function throwByteViewChanged() {
  throw new ERR_INVALID_STATE.TypeError(
    'Byte view was resized or detached after being accepted');
}

function validateByteView(snapshot) {
  const { value } = snapshot;
  if (TypedArrayPrototypeGetByteLength(value) !== snapshot.byteLength) {
    throwByteViewChanged();
  }
  return value;
}

/**
 * Call `method` on `receiver` with the byte view `value`, and throw if the
 * call resized or detached it, as validateByteView() does for a snapshot
 * taken just before the call, without allocating one.
 * @param {Uint8Array} value
 * @param {Function} method
 * @param {object} receiver
 * @returns {any} The result of the call.
 */
function callWithByteView(value, method, receiver) {
  const byteLength = TypedArrayPrototypeGetByteLength(value);
  const result = FunctionPrototypeCall(method, receiver, value);
  if (TypedArrayPrototypeGetByteLength(value) !== byteLength) {
    throwByteViewChanged();
  }
  return result;
}

// States of the hand-written async iterators that replace async generators
// on hot paths.
const kStart = 0;
const kActive = 1;
const kDone = 2;

class QueuedOperation {
  constructor(method, arg, resolve, reject) {
    this.method = method;
    this.arg = arg;
    this.resolve = resolve;
    this.reject = reject;
  }
}

/**
 * Serializes the operations of a hand-written async iterator the way an
 * async generator queues its requests: an operation started while another
 * is in progress waits until it has settled.
 * @returns {{ run: Function, settled: Function, release: Function }}
 */
function createOperationQueue() {
  let busy = false;
  // Created on first use, then kept: queued operations, a RingBuffer of
  // QueuedOperation, since as many can be queued as calls are made.
  let queue = null;

  function drain() {
    if (busy || queue === null || queue.length === 0) return;
    const { method, arg, resolve, reject } = queue.shift();
    busy = true;
    PromisePrototypeThen(method(arg), resolve, reject);
  }

  return {
    __proto__: null,
    // Run `method(arg)`, which returns a promise and must call settled()
    // once its result is known, now or after the operations before it.
    run(method, arg) {
      if (busy || (queue !== null && queue.length !== 0)) {
        const { promise, resolve, reject } = PromiseWithResolvers();
        queue ??= new RingBuffer();
        queue.push(new QueuedOperation(method, arg, resolve, reject));
        return promise;
      }
      busy = true;
      return method(arg);
    },
    settled() {
      busy = false;
      if (queue !== null && queue.length !== 0) {
        PromisePrototypeThen(kResolvedPromise, drain);
      }
    },
    // Like settled(), but start the next queued operation now, as an async
    // generator does once the await of a yield or return completes.
    release() {
      busy = false;
      drain();
    },
    // Whether no operation is running or queued.
    idle() {
      return !busy && (queue === null || queue.length === 0);
    },
  };
}

/**
 * Validate an explicit `budget` option. The spec only requires the default
 * budget to be at least 16384 bytes; any positive explicit budget is valid.
 * @param {unknown} budget
 */
function validateBudget(budget) {
  validateInteger(budget, 'options.budget', 1);
}

/**
 * Append a chunk to `chunks` for later concatenation, and its byteLength to
 * `byteLengths`, to detect that it was resized or detached in the meantime
 * as validateByteView() does, without an object for every chunk.
 * @param {Uint8Array[]} chunks
 * @param {number[]} byteLengths
 * @param {Uint8Array} value
 * @returns {number} The byteLength of `value`.
 */
function recordChunk(chunks, byteLengths, value) {
  const byteLength = TypedArrayPrototypeGetByteLength(value);
  chunks[chunks.length] = value;
  byteLengths[byteLengths.length] = byteLength;
  return byteLength;
}

/**
 * Validate chunks recorded with recordChunk().
 * @param {Uint8Array[]} chunks
 * @param {number[]} byteLengths
 * @returns {Uint8Array[]} `chunks`
 */
function validateRecordedChunks(chunks, byteLengths) {
  for (let i = 0; i < chunks.length; i++) {
    if (TypedArrayPrototypeGetByteLength(chunks[i]) !== byteLengths[i]) {
      throwByteViewChanged();
    }
  }
  return chunks;
}

// A batch accepted for writing one chunk at a time: its chunks, and their
// byteLengths when accepted (see snapshotFixedBatch()).
function FixedBatch(chunks, byteLengths, byteLength) {
  this.chunks = chunks;
  this.byteLengths = byteLengths;
  this.byteLength = byteLength;
}
FixedBatch.prototype = ObjectFreeze({ __proto__: null });

/**
 * Snapshot a batch of Uint8Arrays like createBatchEntry(), without an object
 * for every chunk: their byteLengths are recorded in an array. Check a chunk
 * with checkFixedBatchChunk() before and after using it.
 * @param {Uint8Array[]} chunks
 * @returns {FixedBatch}
 */
function snapshotFixedBatch(chunks) {
  const count = chunks.length;
  const byteLengths = new Array(count);
  let byteLength = 0;
  for (let i = 0; i < count; i++) {
    const length = TypedArrayPrototypeGetByteLength(chunks[i]);
    byteLengths[i] = length;
    byteLength += length;
  }
  return new FixedBatch(ArrayPrototypeSlice(chunks), byteLengths, byteLength);
}

function checkFixedBatchChunk(batch, index) {
  const chunk = batch.chunks[index];
  if (TypedArrayPrototypeGetByteLength(chunk) !== batch.byteLengths[index]) {
    throwByteViewChanged();
  }
  return chunk;
}

// The batch entry of a FixedBatch, to continue writing it asynchronously.
function fixedBatchToEntry(batch) {
  const { chunks, byteLengths } = batch;
  const views = new Array(chunks.length);
  for (let i = 0; i < chunks.length; i++) {
    views[i] = new FixedByteView(chunks[i], byteLengths[i]);
  }
  return new BatchEntry(views, batch.byteLength);
}

function createBatchEntry(chunks) {
  const views = new Array(chunks.length);
  let byteLength = 0;
  for (let i = 0; i < chunks.length; i++) {
    const view = snapshotByteView(chunks[i]);
    views[i] = view;
    byteLength += view.byteLength;
  }
  return new BatchEntry(views, byteLength);
}

/**
 * Split a batch entry into consecutive entries that are each smaller than
 * `limit` bytes, preserving chunk order. A single chunk of `limit` bytes or
 * more forms an entry on its own. Returns `undefined` if no split is needed.
 * @param {{ views: object[], byteLength: number }} entry
 * @param {number} limit
 * @returns {Array<{ views: object[], byteLength: number }>|undefined}
 */
function splitBatchEntry(entry, limit) {
  const { views } = entry;
  if (entry.byteLength < limit || views.length < 2) return undefined;
  const entries = [];
  let current = [];
  let byteLength = 0;
  for (let i = 0; i < views.length; i++) {
    const view = views[i];
    if (current.length > 0 && byteLength + view.byteLength >= limit) {
      entries[entries.length] = new BatchEntry(current, byteLength);
      current = [];
      byteLength = 0;
    }
    current[current.length] = view;
    byteLength += view.byteLength;
  }
  entries[entries.length] = new BatchEntry(current, byteLength);
  return entries;
}

function validateBatchEntry(entry) {
  const chunks = new Array(entry.views.length);
  for (let i = 0; i < entry.views.length; i++) {
    chunks[i] = validateByteView(entry.views[i]);
  }
  return chunks;
}

function copyBytes(chunk) {
  const copy = new Uint8Array(TypedArrayPrototypeGetByteLength(chunk));
  TypedArrayPrototypeSet(copy, chunk);
  return copy;
}

/**
 * Concatenate multiple Uint8Arrays into a single Uint8Array.
 * @param {Uint8Array[]} chunks
 * @returns {Uint8Array}
 */
function concatBytes(chunks) {
  // Empty stream: return zero-length Uint8Array
  if (chunks.length === 0) {
    return new Uint8Array(0);
  }
  // Single chunk: return directly if it covers the entire backing buffer,
  // otherwise return a copy
  if (chunks.length === 1) {
    const chunk = chunks[0];
    // If non-zero offset, skip the remaining buffer checks.
    if (TypedArrayPrototypeGetByteOffset(chunk) === 0) {
      const buf = TypedArrayPrototypeGetBuffer(chunk);
      if (
        !isSharedArrayBuffer(buf) &&
        TypedArrayPrototypeGetByteLength(chunk) ===
          ArrayBufferPrototypeGetByteLength(buf)
      ) {
        return chunk;
      }
    }
    return copyBytes(chunk);
  }
  // Multiple chunks: concatenate
  let totalByteLength = 0;
  for (let i = 0; i < chunks.length; i++) {
    totalByteLength += TypedArrayPrototypeGetByteLength(chunks[i]);
  }
  const concatenated = new Uint8Array(totalByteLength);
  let offset = 0;
  for (let i = 0; i < chunks.length; i++) {
    TypedArrayPrototypeSet(concatenated, chunks[i], offset);
    offset += TypedArrayPrototypeGetByteLength(chunks[i]);
  }
  return concatenated;
}

// Conversion contexts for the per-write paths. The converters only read
// them (to build error messages), so they are shared rather than allocated
// on every write.
const kChunkContext = ObjectFreeze({ __proto__: null, context: 'chunk' });
const kChunksContext = ObjectFreeze({ __proto__: null, context: 'chunks' });
const kWriteOptionsContext =
  ObjectFreeze({ __proto__: null, context: 'options' });

/**
 * Convert an array of chunks (strings or Uint8Arrays) to a Uint8Array[].
 * Always returns a fresh copy of the array.
 * @param {Array<Uint8Array|string>} chunks
 * @returns {Uint8Array[]}
 */
function convertChunks(chunks) {
  chunks = converters.WriterChunkSequence(chunks, kChunksContext);
  const len = chunks.length;
  const result = new Array(len);
  for (let i = 0; i < len; i++) {
    result[i] = toUint8Array(chunks[i]);
  }
  return result;
}

/**
 * Validate Writer options and return options.signal.
 * @param {object|undefined} options
 * @returns {AbortSignal|undefined}
 */
function getWriterSignal(options) {
  // Writes are hot, and converting undefined or null creates an empty
  // dictionary, which has no signal.
  if (options === undefined || options === null) return undefined;
  return converters.WriteOptions(options, kWriteOptionsContext).signal;
}

function toWriterUint8Array(chunk) {
  return toUint8Array(converters.WriterChunk(chunk, kChunkContext));
}

/**
 * Check if a value implements a Symbol-keyed protocol (has a function
 * at the given symbol key).
 * @param {unknown} value
 * @param {symbol} symbol
 * @returns {boolean}
 */
function getProtocolMethod(value, symbol) {
  // Functions are objects too, and may implement the protocols.
  if (value === null ||
      (typeof value !== 'object' && typeof value !== 'function') ||
      !(symbol in value)) {
    return undefined;
  }
  const method = value[symbol];
  return typeof method === 'function' ? method : undefined;
}

function hasProtocol(value, symbol) {
  return getProtocolMethod(value, symbol) !== undefined;
}

const transformRecords = new SafeWeakSet();

/**
 * Read and retain a stateful transform's callable exactly once.
 * @param {unknown} value
 * @returns {Function|object|undefined}
 */
function snapshotTransform(value) {
  if (typeof value === 'function') return value;
  if (transformRecords.has(value)) return value;

  const transform = value?.transform;
  if (typeof transform !== 'function') return undefined;

  const record = {
    __proto__: null,
    transform,
    receiver: value,
    [kValidatedTransform]: value[kValidatedTransform],
  };
  transformRecords.add(record);
  return record;
}

/**
 * Check if a value is a stateful transform object (has a transform method).
 * @param {unknown} value
 * @returns {boolean}
 */
function isTransformObject(value) {
  return transformRecords.has(value) || typeof value?.transform === 'function';
}

/**
 * Check if a value is a valid transform (function or transform object).
 * @param {unknown} value
 * @returns {boolean}
 */
function isTransform(value) {
  return typeof value === 'function' || isTransformObject(value);
}

/**
 * Parse variadic arguments for pull/pullSync.
 * Returns { transforms, options }
 * @param {Array} args
 * @returns {{ transforms: Array, options: unknown }}
 */
function parsePullArgs(args) {
  if (args.length === 0) {
    return { __proto__: null, transforms: [], options: undefined };
  }

  let transforms;
  let options;
  const last = args[args.length - 1];
  const lastTransform = snapshotTransform(last);
  if (lastTransform === undefined) {
    transforms = ArrayPrototypeSlice(args, 0, -1);
    options = last;
  } else {
    transforms = ArrayPrototypeSlice(args);
    transforms[transforms.length - 1] = lastTransform;
    options = undefined;
  }

  for (let i = 0; i < transforms.length; i++) {
    const transform = snapshotTransform(transforms[i]);
    if (transform === undefined) {
      throw new ERR_INVALID_ARG_TYPE(
        `transforms[${i}]`, ['Function', 'Object with transform()'],
        transforms[i]);
    }
    transforms[i] = transform;
  }

  return { __proto__: null, transforms, options };
}

/**
 * Validate backpressure option value.
 * @param {string} value
 */
function validateBackpressure(value) {
  validateOneOf(value, 'options.backpressure', [
    'strict',
    'unbounded',
    'drop-oldest',
    'drop-newest',
  ]);
}

module.exports = {
  IterResult,
  kActive,
  kDone,
  kStart,
  PendingRequest,
  PendingWrite,
  PipelineAbort,
  kMultiConsumerDefaultBudget,
  kNullOnceOption,
  kPushDefaultBudget,
  kResolvedPromise,
  callWithByteView,
  concatBytes,
  createOperationQueue,
  convertChunks,
  checkFixedBatchChunk,
  createBatchEntry,
  fixedBatchToEntry,
  snapshotFixedBatch,
  recordChunk,
  splitBatchEntry,
  getProtocolMethod,
  getWriterSignal,
  getMinCursor,
  hasProtocol,
  isTransform,
  isTransformObject,
  onSignalAbort,
  parsePullArgs,
  raceSignal,
  snapshotTransform,
  toUint8Array,
  toWriterUint8Array,
  validateBackpressure,
  validateBatchEntry,
  validateBudget,
  validateRecordedChunks,
  throwByteViewChanged,
  validateByteView,
  yieldAbortable,
};
