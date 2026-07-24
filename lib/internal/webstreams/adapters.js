'use strict';

const {
  ArrayBufferPrototypeGetDetached,
  ArrayPrototypeFilter,
  BigInt64Array,
  BigUint64Array,
  DataView,
  Float32Array,
  Float64Array,
  FunctionPrototypeCall,
  Int16Array,
  Int32Array,
  Int8Array,
  ObjectKeys,
  PromisePrototypeThen,
  PromiseResolve,
  PromiseWithResolvers,
  SafePromiseAllReturnVoid,
  SafeSet,
  StringPrototypeStartsWith,
  Symbol,
  SymbolDispose,
  TypeError,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  TypedArrayPrototypeGetSymbolToStringTag,
  TypedArrayPrototypeSet,
  Uint16Array,
  Uint32Array,
  Uint8Array,
  Uint8ClampedArray,
  globalThis: {
    Float16Array,
  },
} = primordials;

const { TextEncoder } = require('internal/encoding');

let addAbortListener;
let outgoingMessagePrototypeWrite;

const {
  ReadableStream,
  isReadableStream,
} = require('internal/webstreams/readablestream');

const {
  WritableStream,
  getWritableStreamDefaultControllerSignal,
  isWritableStream,
} = require('internal/webstreams/writablestream');

const {
  CountQueuingStrategy,
  ByteLengthQueuingStrategy,
} = require('internal/webstreams/queuingstrategies');

const {
  Writable,
  Readable,
  Duplex,
  destroy,
} = require('stream');

const {
  isDestroyed,
  isOutgoingMessage,
  isReadable,
  isWritable,
  isWritableEnded,
} = require('internal/streams/utils');

const {
  Buffer,
} = require('buffer');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
  isDataView,
  isSharedArrayBuffer,
  isUint8Array,
} = require('internal/util/types');

const {
  ArrayBufferViewGetBuffer,
  ArrayBufferViewGetByteLength,
  ArrayBufferViewGetByteOffset,
} = require('internal/webstreams/util');

const {
  AbortError,
  ErrnoException,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_STREAM_PREMATURE_CLOSE,
  },
} = require('internal/errors');

const {
  constructSharedArrayBuffer,
  getDeprecationWarningEmitter,
  kEmptyObject,
  normalizeEncoding,
  setOwnProperty,
} = require('internal/util');

const {
  validateBoolean,
  validateFunction,
  validateObject,
  validateOneOf,
} = require('internal/validators');

const {
  WriteWrap,
  ShutdownWrap,
  kReadBytesOrError,
  kLastWriteWasAsync,
  streamBaseState,
} = internalBinding('stream_wrap');

const {
  eos,
  kEosNodeSynchronousCallback,
} = require('internal/streams/end-of-stream');

const { zlib } = internalBinding('constants');
const { UV_EOF } = internalBinding('uv');

const encoder = new TextEncoder();

const kValidateChunk = Symbol('kValidateChunk');
const kDestroyOnSyncError = Symbol('kDestroyOnSyncError');

// Collect all negative (error) ZLIB codes and Z_NEED_DICT
const ZLIB_FAILURES = new SafeSet(
  ArrayPrototypeFilter(
    ObjectKeys(zlib),
    (code) => code === 'Z_NEED_DICT' || zlib[code] < 0,
  ),
);

/**
 * @param {Error|null} cause
 * @returns {Error|null}
 */
function handleKnownInternalErrors(cause) {
  const causeCode = cause?.code;
  switch (true) {
    case causeCode === 'ERR_STREAM_PREMATURE_CLOSE': {
      return new AbortError(undefined, { cause });
    }
    case ZLIB_FAILURES.has(causeCode):
    // Brotli decoder error codes are formatted as 'ERR_' +
    // BrotliDecoderErrorString(), where the latter returns strings like
    // '_ERROR_FORMAT_...', '_ERROR_ALLOC_...', '_ERROR_UNREACHABLE', etc.
    // The resulting JS error codes all start with 'ERR__ERROR_'.
    // Falls through
    case causeCode != null &&
      StringPrototypeStartsWith(causeCode, 'ERR__ERROR_'): {
      // eslint-disable-next-line no-restricted-syntax
      const error = new TypeError(undefined, { cause });
      setOwnProperty(error, 'code', causeCode);
      return error;
    }
    default:
      return cause;
  }
}

const noop = () => {};

function cloneArrayBufferView(view) {
  const viewIsDataView = isDataView(view);
  const buffer = ArrayBufferViewGetBuffer(view);

  // A detached backing buffer cannot be changed by the caller. Passing the
  // original view through also preserves the native stream's validation.
  if (!isSharedArrayBuffer(buffer) &&
      ArrayBufferPrototypeGetDetached(buffer)) {
    return view;
  }

  const byteLength = ArrayBufferViewGetByteLength(view);
  const byteOffset = ArrayBufferViewGetByteOffset(view);
  const copiedBytes = new Uint8Array(
    isSharedArrayBuffer(buffer) ?
      constructSharedArrayBuffer(byteLength) :
      byteLength,
  );
  TypedArrayPrototypeSet(
    copiedBytes,
    new Uint8Array(buffer, byteOffset, byteLength),
  );
  const copiedBuffer = ArrayBufferViewGetBuffer(copiedBytes);

  if (viewIsDataView) {
    return new DataView(copiedBuffer);
  }

  switch (TypedArrayPrototypeGetSymbolToStringTag(view)) {
    case 'BigInt64Array':
      return new BigInt64Array(copiedBuffer);
    case 'BigUint64Array':
      return new BigUint64Array(copiedBuffer);
    case 'Float16Array':
      return new Float16Array(copiedBuffer);
    case 'Float32Array':
      return new Float32Array(copiedBuffer);
    case 'Float64Array':
      return new Float64Array(copiedBuffer);
    case 'Int8Array':
      return new Int8Array(copiedBuffer);
    case 'Int16Array':
      return new Int16Array(copiedBuffer);
    case 'Int32Array':
      return new Int32Array(copiedBuffer);
    case 'Uint8Array':
      return Buffer.isBuffer(view) ? Buffer.from(copiedBuffer) : copiedBytes;
    case 'Uint8ClampedArray':
      return new Uint8ClampedArray(copiedBuffer);
    case 'Uint16Array':
      return new Uint16Array(copiedBuffer);
    case 'Uint32Array':
      return new Uint32Array(copiedBuffer);
  }
}

/**
 * @typedef {import('../../stream').Writable} Writable
 * @typedef {import('../../stream').Readable} Readable
 * @typedef {import('./writablestream').WritableStream} WritableStream
 * @typedef {import('./readablestream').ReadableStream} ReadableStream
 */

/**
 * @typedef {import('../abort_controller').AbortSignal} AbortSignal
 */

/**
 * @param {Writable} streamWritable
 * @param {object} [options]
 * @param {Function} [writablePrototypeWrite]
 * @returns {WritableStream}
 */
function newWritableStreamFromStreamWritable(
  streamWritable,
  options = kEmptyObject,
  writablePrototypeWrite,
) {
  // Not using the internal/streams/utils isWritableNodeStream utility
  // here because it will return false if streamWritable is a Duplex
  // whose writable option is false. For a Duplex that is not writable,
  // we want it to pass this check but return a closed WritableStream.
  // We check if the given stream is a stream.Writable or http.OutgoingMessage
  const checkIfWritableOrOutgoingMessage =
    streamWritable &&
    typeof streamWritable?.write === 'function' &&
    typeof streamWritable?.on === 'function';
  if (!checkIfWritableOrOutgoingMessage) {
    throw new ERR_INVALID_ARG_TYPE(
      'streamWritable',
      'stream.Writable',
      streamWritable,
    );
  }

  if (isDestroyed(streamWritable) || !isWritable(streamWritable)) {
    const writable = new WritableStream();
    writable.close();
    return writable;
  }

  const highWaterMark = streamWritable.writableHighWaterMark;
  const strategy =
    streamWritable.writableObjectMode ?
      new CountQueuingStrategy({ highWaterMark }) :
      {
        highWaterMark,
        size(chunk) {
          return chunk?.byteLength ?? chunk?.length ?? 1;
        },
      };

  let controller;
  let pendingWriteState;
  let isTerminated = false;
  let terminalReason;
  let closed;
  let abortSignal;
  let abortDisposable;

  function disposeAbortListener() {
    abortDisposable?.[SymbolDispose]();
    abortDisposable = undefined;
  }

  function recordTerminalReason(reason) {
    if (!isTerminated) {
      isTerminated = true;
      terminalReason = reason;
    }
  }

  function resolvePendingWrite(state) {
    if (pendingWriteState !== state) {
      return;
    }
    pendingWriteState = undefined;
    disposeAbortListener();
    state.deferred.resolve();
  }

  function rejectPendingWrite(error) {
    const state = pendingWriteState;
    if (state === undefined) {
      return;
    }
    pendingWriteState = undefined;
    disposeAbortListener();
    state.deferred.reject(error);
  }

  function listenForPendingAbort() {
    addAbortListener ??=
      require('internal/events/abort_listener').addAbortListener;
    abortDisposable = addAbortListener(abortSignal, () => {
      // Release an in-flight write so the sink's abort algorithm can run.
      // The abort algorithm is solely responsible for destroying the stream.
      recordTerminalReason(abortSignal.reason);
      rejectPendingWrite(abortSignal.reason);
    });
  }

  function maybeResolvePendingWrite(state) {
    if (state.writeComplete && state.backpressureCleared) {
      resolvePendingWrite(state);
    }
  }

  function markWriteComplete(state, error) {
    if (pendingWriteState !== state) {
      return;
    }
    if (error != null) {
      rejectPendingWrite(error);
      return;
    }
    state.writeComplete = true;
    maybeResolvePendingWrite(state);
  }

  function createPendingWriteState(waitForWriteCallback) {
    const state = {
      __proto__: null,
      deferred: PromiseWithResolvers(),
      writeComplete: !waitForWriteCallback,
      backpressureCleared: false,
    };
    pendingWriteState = state;
    listenForPendingAbort();
    return state;
  }

  function onDrain() {
    const state = pendingWriteState;
    if (state === undefined) {
      return;
    }
    state.backpressureCleared = true;
    maybeResolvePendingWrite(state);
  }

  function onWrite(error) {
    const state = pendingWriteState;
    if (state === undefined) {
      return;
    }
    if (error != null) {
      error = handleKnownInternalErrors(error);
    }
    markWriteComplete(state, error);
  }

  function throwSyncWriteError(error) {
    disposeAbortListener();
    // When the kDestroyOnSyncError flag is set (e.g. for
    // CompressionStream), a sync throw must also destroy the
    // stream so the readable side is errored too. Without this
    // the readable side hangs forever. This replicates the
    // TransformStream semantics: error both sides on any throw
    // in the transform path.
    if (options[kDestroyOnSyncError]) {
      destroy(streamWritable, error);
    }
    throw error;
  }

  function throwIfTerminated() {
    if (abortSignal.aborted) {
      recordTerminalReason(abortSignal.reason);
    }
    if (isTerminated) {
      throw terminalReason;
    }
  }

  function writeChunk(
    writeMethod,
    chunk,
    needDrainBeforeWrite,
    waitForWriteCallback,
  ) {
    if (!waitForWriteCallback && !needDrainBeforeWrite) {
      try {
        const writeReturn = FunctionPrototypeCall(
          writeMethod,
          streamWritable,
          chunk,
        );
        throwIfTerminated();
        if (controller === undefined ||
            writeReturn ||
            !streamWritable.writableNeedDrain) {
          return;
        }
      } catch (error) {
        throwSyncWriteError(error);
      }

      const state = createPendingWriteState(false);
      return state.deferred.promise;
    }

    const state = createPendingWriteState(waitForWriteCallback);
    try {
      // The return value only reports backpressure. The callback reports when
      // the chunk has been handled and can safely be mutated by the caller.
      const writeReturn = waitForWriteCallback ?
        FunctionPrototypeCall(writeMethod, streamWritable, chunk, onWrite) :
        FunctionPrototypeCall(writeMethod, streamWritable, chunk);
      throwIfTerminated();
      const shouldWaitForDrain =
        (needDrainBeforeWrite || !writeReturn) &&
        streamWritable.writableNeedDrain;
      state.backpressureCleared = !shouldWaitForDrain;
      maybeResolvePendingWrite(state);
    } catch (error) {
      resolvePendingWrite(state);
      PromisePrototypeThen(state.deferred.promise, undefined, noop);
      throwSyncWriteError(error);
    }

    return state.deferred.promise;
  }

  const cleanup = eos(streamWritable, (error) => {
    error = handleKnownInternalErrors(error);

    cleanup();
    disposeAbortListener();
    // This is a protection against non-standard, legacy streams
    // that happen to emit an error event again after finished is called.
    streamWritable.on('error', () => {});
    if (error != null) {
      recordTerminalReason(error);
      rejectPendingWrite(error);
      // If closed is not undefined, the error is happening
      // after the WritableStream close has already started.
      // We need to reject it here.
      if (closed !== undefined) {
        closed.reject(error);
        closed = undefined;
      }
      controller.error(error);
      controller = undefined;
      return;
    }

    if (closed !== undefined) {
      closed.resolve();
      closed = undefined;
      return;
    }
    const abortError = new AbortError();
    recordTerminalReason(abortError);
    rejectPendingWrite(abortError);
    controller.error(abortError);
    controller = undefined;
  });

  streamWritable.on('drain', onDrain);

  return new WritableStream({
    start(c) {
      controller = c;
      abortSignal = getWritableStreamDefaultControllerSignal(c);
    },

    write(chunk) {
      let writeMethod;
      let needDrainBeforeWrite;
      let waitForWriteCallback;
      try {
        options[kValidateChunk]?.(chunk);
        writeMethod = streamWritable.write;
        const writableState = streamWritable._writableState;
        const objectMode = streamWritable.writableObjectMode;
        const isBufferSourceChunk =
          isAnyArrayBuffer(chunk) || isArrayBufferView(chunk);
        if (!objectMode && isAnyArrayBuffer(chunk)) {
          chunk = new Uint8Array(chunk);
        }
        needDrainBeforeWrite = streamWritable.writableNeedDrain;
        const isArrayBufferViewChunk = isArrayBufferView(chunk);
        if (!objectMode &&
            isOutgoingMessage(streamWritable) &&
            isArrayBufferViewChunk &&
            !isUint8Array(chunk)) {
          outgoingMessagePrototypeWrite ??=
            require('_http_outgoing').outgoingMessagePrototypeWrite;
          if (writeMethod === outgoingMessagePrototypeWrite) {
            throw new ERR_INVALID_ARG_TYPE(
              'chunk', ['string', 'Buffer', 'Uint8Array'], chunk);
          }
        }
        const canAwaitWriteCallback =
          streamWritable instanceof Writable &&
          writeMethod === writablePrototypeWrite &&
          streamWritable._readableState === undefined &&
          writableState?.corked === 0;
        waitForWriteCallback =
          isBufferSourceChunk && canAwaitWriteCallback;
        if (!objectMode &&
            isArrayBufferViewChunk &&
            !waitForWriteCallback) {
          // Duplex streams can retain chunks in their readable side or
          // forward them after their own write callback. A corked stream does
          // not invoke callbacks until uncork() or end(), and streams that
          // override write() may not support callbacks at all. Preserve the
          // existing completion timing for those cases by passing a private,
          // same-type copy to the native stream.
          chunk = cloneArrayBufferView(chunk);
        }
      } catch (error) {
        throwSyncWriteError(error);
      }

      return writeChunk(
        writeMethod,
        chunk,
        needDrainBeforeWrite,
        waitForWriteCallback,
      );
    },

    abort(reason) {
      disposeAbortListener();
      destroy(streamWritable, reason);
    },

    close() {
      if (closed === undefined && !isWritableEnded(streamWritable)) {
        closed = PromiseWithResolvers();
        try {
          streamWritable.end();
        } catch (error) {
          closed = undefined;
          disposeAbortListener();
          throw error;
        }
        return closed.promise;
      }

      controller = undefined;
      disposeAbortListener();
      return PromiseResolve();
    },
  }, strategy);
}

/**
 * @param {WritableStream} writableStream
 * @param {{
 *   decodeStrings? : boolean,
 *   highWaterMark? : number,
 *   objectMode? : boolean,
 *   signal? : AbortSignal,
 * }} [options]
 * @returns {Writable}
 */
function newStreamWritableFromWritableStream(writableStream, options = kEmptyObject) {
  if (!isWritableStream(writableStream)) {
    throw new ERR_INVALID_ARG_TYPE(
      'writableStream',
      'WritableStream',
      writableStream);
  }

  validateObject(options, 'options');
  const {
    highWaterMark,
    decodeStrings = true,
    objectMode = false,
    signal,
  } = options;

  validateBoolean(objectMode, 'options.objectMode');
  validateBoolean(decodeStrings, 'options.decodeStrings');

  const writer = writableStream.getWriter();
  let closed = false;

  const writable = new Writable({
    highWaterMark,
    objectMode,
    decodeStrings,
    signal,

    writev(chunks, callback) {
      function done(error) {
        try {
          callback(error);
        } catch (error) {
          // In a next tick because this is happening within
          // a promise context, and if there are any errors
          // thrown we don't want those to cause an unhandled
          // rejection. Let's just escape the promise and
          // handle it separately.
          process.nextTick(() => destroy(writable, error));
        }
      }

      PromisePrototypeThen(
        PromisePrototypeThen(
          writer.ready,
          () => SafePromiseAllReturnVoid(
            chunks,
            (data) => writer.write(data.chunk))),
        done,
        done);
    },

    write(chunk, encoding, callback) {
      if (typeof chunk === 'string' && decodeStrings && !objectMode) {
        const enc = normalizeEncoding(encoding);

        if (enc === 'utf8') {
          chunk = encoder.encode(chunk);
        } else {
          chunk = Buffer.from(chunk, encoding);
          chunk = new Uint8Array(
            TypedArrayPrototypeGetBuffer(chunk),
            TypedArrayPrototypeGetByteOffset(chunk),
            TypedArrayPrototypeGetByteLength(chunk),
          );
        }
      }

      function done(error) {
        try {
          callback(error);
        } catch (error) {
          destroy(writable, error);
        }
      }

      PromisePrototypeThen(
        PromisePrototypeThen(writer.ready, () => writer.write(chunk)),
        done,
        done);
    },

    destroy(error, callback) {
      function done() {
        try {
          callback(error);
        } catch (error) {
          // In a next tick because this is happening within
          // a promise context, and if there are any errors
          // thrown we don't want those to cause an unhandled
          // rejection. Let's just escape the promise and
          // handle it separately.
          process.nextTick(() => { throw error; });
        }
      }

      if (!closed) {
        if (error != null) {
          PromisePrototypeThen(
            writer.abort(error),
            done,
            done);
        } else {
          PromisePrototypeThen(
            writer.close(),
            done,
            done);
        }
        return;
      }

      done();
    },

    final(callback) {
      function done(error) {
        try {
          callback(error);
        } catch (error) {
          // In a next tick because this is happening within
          // a promise context, and if there are any errors
          // thrown we don't want those to cause an unhandled
          // rejection. Let's just escape the promise and
          // handle it separately.
          process.nextTick(() => destroy(writable, error));
        }
      }

      if (!closed) {
        PromisePrototypeThen(
          writer.close(),
          done,
          done);
      }
    },
  });

  PromisePrototypeThen(
    writer.closed,
    () => {
      // If the WritableStream closes before the stream.Writable has been
      // ended, we signal an error on the stream.Writable.
      closed = true;
      if (!isWritableEnded(writable))
        destroy(writable, new ERR_STREAM_PREMATURE_CLOSE());
    },
    (error) => {
      // If the WritableStream errors before the stream.Writable has been
      // destroyed, signal an error on the stream.Writable.
      closed = true;
      destroy(writable, error);
    });

  return writable;
}

const kErrorSentinelAttached = Symbol('kErrorSentinelAttached');

/**
 * @typedef {import('./queuingstrategies').QueuingStrategy} QueuingStrategy
 * @param {Readable} streamReadable
 * @param {{
 *  strategy? : QueuingStrategy
 *  type? : 'bytes',
 * }} [options]
 * @returns {ReadableStream}
 */
function newReadableStreamFromStreamReadable(streamReadable, options = kEmptyObject) {
  // Not using the internal/streams/utils isReadableNodeStream utility
  // here because it will return false if streamReadable is a Duplex
  // whose readable option is false. For a Duplex that is not readable,
  // we want it to pass this check but return a closed ReadableStream.
  if (typeof streamReadable?._readableState !== 'object') {
    throw new ERR_INVALID_ARG_TYPE(
      'streamReadable',
      'stream.Readable',
      streamReadable);
  }
  validateObject(options, 'options');
  if (options.type !== undefined) {
    validateOneOf(options.type, 'options.type', ['bytes', undefined]);
  }

  const isBYOB = options.type === 'bytes';
  let controller;
  let wasCanceled = false;
  let strategy;

  /** @type {UnderlyingSource} */
  const underlyingSource = {
    __proto__: null,
    type: isBYOB ? 'bytes' : undefined,
    start(c) { controller = c; },
    cancel(reason) {
      wasCanceled = true;
      destroy(streamReadable, reason);
    },
  };

  const readable = isReadable(streamReadable);
  const objectMode = streamReadable.readableObjectMode;
  if (readable) {
    underlyingSource.pull = function pull() {
      streamReadable.resume();
    };

    const highWaterMark = streamReadable.readableHighWaterMark;
    strategy = isBYOB ? { highWaterMark } :
      options.strategy ?? new (objectMode ? CountQueuingStrategy : ByteLengthQueuingStrategy)({ highWaterMark });
  }
  const readableStream = new ReadableStream(underlyingSource, strategy);

  // When adapting a Duplex as a ReadableStream, readable completion should not
  // wait for a half-open writable side to finish as well.
  let cleanup = noop;
  cleanup = eos(streamReadable, {
    __proto__: null,
    writable: false,
    [kEosNodeSynchronousCallback]: true,
  }, (error) => {
    error = handleKnownInternalErrors(error);

    // If eos calls the callback synchronously, cleanup is still a no-op here.
    cleanup();

    if (!(kErrorSentinelAttached in streamReadable)) {
      // This is a protection against non-standard, legacy streams
      // that happen to emit an error event again after finished is called.
      streamReadable.on('error', noop);
      streamReadable[kErrorSentinelAttached] = true;
    }
    if (wasCanceled) {
      return;
    }
    wasCanceled = true;
    if (error)
      return controller.error(error);
    controller.close();
    if (isBYOB)
      controller.byobRequest?.respond(0);
  });

  if (wasCanceled) {
    // `eos` called the callback synchronously
    cleanup();
  } else if (readable) {
    streamReadable.pause();

    streamReadable.on('data', function onData(chunk) {
      // Copy the Buffer to detach it from the pool.
      if (Buffer.isBuffer(chunk) && !objectMode)
        chunk = new Uint8Array(chunk);
      controller.enqueue(chunk);
      if (controller.desiredSize <= 0)
        streamReadable.pause();
    });
  }

  return readableStream;
}

/**
 * @param {ReadableStream} readableStream
 * @param {{
 *   highWaterMark? : number,
 *   encoding? : string,
 *   objectMode? : boolean,
 *   signal? : AbortSignal,
 * }} [options]
 * @returns {Readable}
 */
function newStreamReadableFromReadableStream(readableStream, options = kEmptyObject) {
  if (!isReadableStream(readableStream)) {
    throw new ERR_INVALID_ARG_TYPE(
      'readableStream',
      'ReadableStream',
      readableStream);
  }

  validateObject(options, 'options');
  const {
    highWaterMark,
    encoding,
    objectMode = false,
    signal,
  } = options;

  if (encoding !== undefined && !Buffer.isEncoding(encoding))
    throw new ERR_INVALID_ARG_VALUE('options.encoding', encoding);
  validateBoolean(objectMode, 'options.objectMode');

  const reader = readableStream.getReader();
  let closed = false;

  const readable = new Readable({
    objectMode,
    highWaterMark,
    encoding,
    signal,

    read() {
      PromisePrototypeThen(
        reader.read(),
        (chunk) => {
          if (chunk.done) {
            // Value should always be undefined here.
            readable.push(null);
          } else {
            readable.push(chunk.value);
          }
        },
        (error) => destroy(readable, error));
    },

    destroy(error, callback) {
      function done() {
        try {
          callback(error);
        } catch (error) {
          // In a next tick because this is happening within
          // a promise context, and if there are any errors
          // thrown we don't want those to cause an unhandled
          // rejection. Let's just escape the promise and
          // handle it separately.
          process.nextTick(() => { throw error; });
        }
      }

      if (!closed) {
        PromisePrototypeThen(
          reader.cancel(error),
          done,
          done);
        return;
      }
      done();
    },
  });

  PromisePrototypeThen(
    reader.closed,
    () => {
      closed = true;
    },
    (error) => {
      closed = true;
      destroy(readable, error);
    });

  return readable;
}

const emitDEP0201 = getDeprecationWarningEmitter(
  'DEP0201',
  "Passing 'options.type' to Duplex.toWeb() is deprecated. " +
      "To specify the ReadableStream type, use 'options.readableType'.",
  newReadableWritablePairFromDuplex);

/**
 * @typedef {import('./readablestream').ReadableWritablePair
 * } ReadableWritablePair
 * @typedef {import('../../stream').Duplex} Duplex
 */

/**
 * @param {Duplex} duplex
 * @param {{ readableType?: 'bytes' }} [options]
 * @returns {ReadableWritablePair}
 */
function newReadableWritablePairFromDuplex(duplex, options = kEmptyObject) {
  // Not using the internal/streams/utils isWritableNodeStream and
  // isReadableNodeStream utilities here because they will return false
  // if the duplex was created with writable or readable options set to
  // false. Instead, we'll check the readable and writable state after
  // and return closed WritableStream or closed ReadableStream as
  // necessary.
  if (typeof duplex?._writableState !== 'object' ||
      typeof duplex?._readableState !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('duplex', 'stream.Duplex', duplex);
  }

  validateObject(options, 'options');

  const readableOptions = {
    __proto__: null,
    type: options.readableType,
  };

  if (options.readableType == null && options.type != null) {
    // 'options.type' is a deprecated alias for 'options.readableType'
    emitDEP0201();
    readableOptions.type = options.type;
  }

  if (isDestroyed(duplex)) {
    const writable = new WritableStream();
    const readable = new ReadableStream({ type: readableOptions.type });
    writable.close();
    readable.cancel();
    return { readable, writable };
  }

  const writableOptions = {
    __proto__: null,
    [kValidateChunk]: options[kValidateChunk],
    [kDestroyOnSyncError]: options[kDestroyOnSyncError],
  };

  const writable =
    isWritable(duplex) ?
      newWritableStreamFromStreamWritable(duplex, writableOptions) :
      new WritableStream();

  if (!isWritable(duplex))
    writable.close();

  const readable =
    isReadable(duplex) ?
      newReadableStreamFromStreamReadable(duplex, readableOptions) :
      new ReadableStream({ type: readableOptions.type });

  if (!isReadable(duplex))
    readable.cancel();

  return { writable, readable };
}

/**
 * @param {ReadableWritablePair} pair
 * @param {{
 *   allowHalfOpen? : boolean,
 *   decodeStrings? : boolean,
 *   encoding? : string,
 *   highWaterMark? : number,
 *   objectMode? : boolean,
 *   signal? : AbortSignal,
 * }} [options]
 * @returns {Duplex}
 */
function newStreamDuplexFromReadableWritablePair(pair = kEmptyObject, options = kEmptyObject) {
  validateObject(pair, 'pair');
  const {
    readable: readableStream,
    writable: writableStream,
  } = pair;

  if (!isReadableStream(readableStream)) {
    throw new ERR_INVALID_ARG_TYPE(
      'pair.readable',
      'ReadableStream',
      readableStream);
  }
  if (!isWritableStream(writableStream)) {
    throw new ERR_INVALID_ARG_TYPE(
      'pair.writable',
      'WritableStream',
      writableStream);
  }

  validateObject(options, 'options');
  const {
    allowHalfOpen = false,
    objectMode = false,
    encoding,
    decodeStrings = true,
    highWaterMark,
    signal,
  } = options;

  validateBoolean(objectMode, 'options.objectMode');
  if (encoding !== undefined && !Buffer.isEncoding(encoding))
    throw new ERR_INVALID_ARG_VALUE('options.encoding', encoding);

  const writer = writableStream.getWriter();
  const reader = readableStream.getReader();
  let writableClosed = false;
  let readableClosed = false;

  const duplex = new Duplex({
    allowHalfOpen,
    highWaterMark,
    objectMode,
    encoding,
    decodeStrings,
    signal,

    writev(chunks, callback) {
      function done(error) {
        try {
          callback(error);
        } catch (error) {
          // In a next tick because this is happening within
          // a promise context, and if there are any errors
          // thrown we don't want those to cause an unhandled
          // rejection. Let's just escape the promise and
          // handle it separately.
          process.nextTick(() => destroy(duplex, error));
        }
      }

      PromisePrototypeThen(
        PromisePrototypeThen(
          writer.ready,
          () => SafePromiseAllReturnVoid(
            chunks,
            (data) => writer.write(data.chunk))),
        done,
        done);
    },

    write(chunk, encoding, callback) {
      if (typeof chunk === 'string' && decodeStrings && !objectMode) {
        const enc = normalizeEncoding(encoding);

        if (enc === 'utf8') {
          chunk = encoder.encode(chunk);
        } else {
          chunk = Buffer.from(chunk, encoding);
          chunk = new Uint8Array(
            TypedArrayPrototypeGetBuffer(chunk),
            TypedArrayPrototypeGetByteOffset(chunk),
            TypedArrayPrototypeGetByteLength(chunk),
          );
        }
      }

      function done(error) {
        try {
          callback(error);
        } catch (error) {
          destroy(duplex, error);
        }
      }

      PromisePrototypeThen(
        PromisePrototypeThen(writer.ready, () => writer.write(chunk)),
        done,
        done);
    },

    final(callback) {
      function done(error) {
        try {
          callback(error);
        } catch (error) {
          // In a next tick because this is happening within
          // a promise context, and if there are any errors
          // thrown we don't want those to cause an unhandled
          // rejection. Let's just escape the promise and
          // handle it separately.
          process.nextTick(() => destroy(duplex, error));
        }
      }

      if (!writableClosed) {
        PromisePrototypeThen(
          writer.close(),
          done,
          done);
      }
    },

    read() {
      PromisePrototypeThen(
        reader.read(),
        (chunk) => {
          if (chunk.done) {
            duplex.push(null);
          } else {
            duplex.push(chunk.value);
          }
        },
        (error) => destroy(duplex, error));
    },

    destroy(error, callback) {
      function done() {
        try {
          callback(error);
        } catch (error) {
          // In a next tick because this is happening within
          // a promise context, and if there are any errors
          // thrown we don't want those to cause an unhandled
          // rejection. Let's just escape the promise and
          // handle it separately.
          process.nextTick(() => { throw error; });
        }
      }

      async function closeWriter() {
        if (!writableClosed)
          await writer.abort(error);
      }

      async function closeReader() {
        if (!readableClosed)
          await reader.cancel(error);
      }

      if (!writableClosed || !readableClosed) {
        PromisePrototypeThen(
          SafePromiseAllReturnVoid([
            closeWriter(),
            closeReader(),
          ]),
          done,
          done);
        return;
      }

      done();
    },
  });

  PromisePrototypeThen(
    writer.closed,
    () => {
      writableClosed = true;
      if (!isWritableEnded(duplex))
        destroy(duplex, new ERR_STREAM_PREMATURE_CLOSE());
    },
    (error) => {
      writableClosed = true;
      readableClosed = true;
      destroy(duplex, error);
    });

  PromisePrototypeThen(
    reader.closed,
    () => {
      readableClosed = true;
    },
    (error) => {
      writableClosed = true;
      readableClosed = true;
      destroy(duplex, error);
    });

  return duplex;
}

/**
 * @typedef {import('./queuingstrategies').QueuingStrategy} QueuingStrategy
 * @param {object} streamBase
 * @param {QueuingStrategy} strategy
 * @returns {WritableStream}
 */
function newWritableStreamFromStreamBase(streamBase, strategy) {
  validateObject(streamBase, 'streamBase');

  let current;

  function createWriteWrap(controller, promise) {
    const req = new WriteWrap();
    req.handle = streamBase;
    req.oncomplete = onWriteComplete;
    req.async = false;
    req.bytes = 0;
    req.buffer = null;
    req.controller = controller;
    req.promise = promise;
    return req;
  }

  function onWriteComplete(status) {
    if (status < 0) {
      const error = new ErrnoException(status, 'write', this.error);
      this.promise.reject(error);
      this.controller.error(error);
      return;
    }
    this.promise.resolve();
  }

  function doWrite(chunk, controller) {
    const promise = PromiseWithResolvers();
    let ret;
    let req;
    try {
      req = createWriteWrap(controller, promise);
      ret = streamBase.writeBuffer(req, chunk);
      if (streamBaseState[kLastWriteWasAsync])
        req.buffer = chunk;
      req.async = !!streamBaseState[kLastWriteWasAsync];
    } catch (error) {
      promise.reject(error);
    }

    if (ret !== 0)
      promise.reject(new ErrnoException(ret, 'write', req));
    else if (!req.async)
      promise.resolve();

    return promise.promise;
  }

  return new WritableStream({
    write(chunk, controller) {
      current = current !== undefined ?
        PromisePrototypeThen(
          current,
          () => doWrite(chunk, controller),
          (error) => controller.error(error)) :
        doWrite(chunk, controller);
      return current;
    },

    close() {
      const promise = PromiseWithResolvers();
      const req = new ShutdownWrap();
      req.oncomplete = () => promise.resolve();
      const err = streamBase.shutdown(req);
      if (err === 1)
        promise.resolve();
      return promise.promise;
    },
  }, strategy);
}

/**
 * @param {StreamBase} streamBase
 * @param {QueuingStrategy} strategy
 * @returns {ReadableStream}
 */
function newReadableStreamFromStreamBase(streamBase, strategy, options = kEmptyObject) {
  validateObject(streamBase, 'streamBase');
  validateObject(options, 'options');

  const {
    ondone = () => {},
  } = options;

  if (typeof streamBase.onread === 'function')
    throw new ERR_INVALID_STATE('StreamBase already has a consumer');

  validateFunction(ondone, 'options.ondone');

  let controller;

  streamBase.onread = (arrayBuffer) => {
    const nread = streamBaseState[kReadBytesOrError];

    if (nread === 0)
      return;

    try {
      if (nread === UV_EOF) {
        controller.close();
        streamBase.readStop();
        try {
          ondone();
        } catch (error) {
          controller.error(error);
        }
        return;
      }

      controller.enqueue(arrayBuffer);

      if (controller.desiredSize <= 0)
        streamBase.readStop();
    } catch (error) {
      controller.error(error);
      streamBase.readStop();
    }
  };

  return new ReadableStream({
    start(c) { controller = c; },

    pull() {
      streamBase.readStart();
    },

    cancel() {
      const promise = PromiseWithResolvers();
      try {
        ondone();
      } catch (error) {
        promise.reject(error);
        return promise.promise;
      }
      const req = new ShutdownWrap();
      req.oncomplete = () => promise.resolve();
      const err = streamBase.shutdown(req);
      if (err === 1)
        promise.resolve();
      return promise.promise;
    },
  }, strategy);
}

module.exports = {
  newWritableStreamFromStreamWritable,
  newReadableStreamFromStreamReadable,
  newStreamWritableFromWritableStream,
  newStreamReadableFromReadableStream,
  newReadableWritablePairFromDuplex,
  newStreamDuplexFromReadableWritablePair,
  newWritableStreamFromStreamBase,
  newReadableStreamFromStreamBase,
  kValidateChunk,
  kDestroyOnSyncError,
};
