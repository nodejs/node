'use strict';

const {
  FunctionPrototypeBind,
  FunctionPrototypeCall,
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  DOMException,
} = internalBinding('messaging');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
  kEnumerableProperty,
  SideEffectFreeRegExpPrototypeSymbolReplace,
} = require('internal/util');

const {
  validateObject,
  kValidateObjectAllowObjects,
  kValidateObjectAllowObjectsAndNull,
} = require('internal/validators');

const {
  MessageChannel,
} = require('internal/worker/io');

const {
  kDeserialize,
  kTransfer,
  kTransferList,
  markTransferMode,
} = require('internal/worker/js_transferable');

const {
  Queue,
  createPromiseCallbackNoParams,
  createPromiseCallback1Param,
  createPromiseCallback2Params,
  customInspect,
  defaultSizeAlgorithm,
  dequeueValue,
  enqueueValueWithSize,
  extractHighWaterMark,
  extractSizeAlgorithm,
  getNonWritablePropertyDescriptor,
  isBrandCheck,
  isPromisePending,
  kEmptyQueue,
  kState,
  kType,
  lazyTransfer,
  nonOpCancel,
  nonOpStart,
  nonOpWrite,
  peekQueueValue,
  rejectedHandledRecord,
  resetQueue,
  resolvedRecord,
  setPromiseHandled,
} = require('internal/webstreams/util');

const {
  kIsClosedPromise,
  kIsErrored,
  kIsWritable,
  kControllerErrorFunction,
} = require('internal/streams/utils');

const {
  AbortController,
} = require('internal/abort_controller');

const {
  queueMicrotask,
} = require('internal/process/task_queues');

const assert = require('internal/assert');

const kAbort = Symbol('kAbort');
const kCloseSentinel = Symbol('kCloseSentinel');
const kError = Symbol('kError');
const kSkipThrow = Symbol('kSkipThrow');

// Shared sentinels for the "no pending request" state records. These
// records are only ever replaced wholesale and never mutated in place,
// so single shared instances are safe and avoid an allocation on every
// state reset (one per write on the hot path).
const kNilRequest = {
  __proto__: null,
  promise: undefined,
  resolve: undefined,
  reject: undefined,
};
const kNilPendingAbortRequest = {
  __proto__: null,
  abort: kNilRequest,
  reason: undefined,
  wasAlreadyErroring: false,
};

let releasedError;

function lazyWritableReleasedError() {
  if (releasedError) {
    return releasedError;
  }
  const userModuleRegExp = /^ {4}at (?:[^/\\(]+ \()(?!node:(.+):\d+:\d+\)$).*/gm;

  releasedError = new ERR_INVALID_STATE.TypeError('Writer has been released');
  // Avoid V8 leak and remove userland stackstrace
  releasedError.stack = SideEffectFreeRegExpPrototypeSymbolReplace(userModuleRegExp, releasedError.stack, '');
  return releasedError;
}

/**
 * @typedef {import('../abort_controller').AbortSignal} AbortSignal
 * @typedef {import('./queuingstrategies').QueuingStrategy
 * } QueuingStrategy
 * @typedef {import('./queuingstrategies').QueuingStrategySize
 * } QueuingStrategySize
 */

/**
 * @callback UnderlyingSinkStartCallback
 * @param {WritableStreamDefaultController} controller
 */

/**
 * @callback UnderlyingSinkWriteCallback
 * @param {any} chunk
 * @param {WritableStreamDefaultController} controller
 * @returns {Promise<void>}
 */

/**
 * @callback UnderlyingSinkCloseCallback
 * @returns {Promise<void>}
 */

/**
 * @callback UnderlyingSinkAbortCallback
 * @param {any} reason
 * @returns {Promise<void>}
 */

/**
 * @typedef {{
 *   start? : UnderlyingSinkStartCallback,
 *   write? : UnderlyingSinkWriteCallback,
 *   close? : UnderlyingSinkCloseCallback,
 *   abort? : UnderlyingSinkAbortCallback,
 *   type? : any,
 * }} UnderlyingSink
 */

class WritableStream {
  [kType] = 'WritableStream';

  /**
   * @param {UnderlyingSink} [sink]
   * @param {QueuingStrategy} [strategy]
   */
  constructor(sink = kEmptyObject, strategy = kEmptyObject) {
    markTransferMode(this, false, true);
    validateObject(sink, 'sink', kValidateObjectAllowObjects);
    validateObject(strategy, 'strategy', kValidateObjectAllowObjectsAndNull);
    const type = sink?.type;
    if (type !== undefined)
      throw new ERR_INVALID_ARG_VALUE.RangeError('type', type);

    this[kState] = createWritableStreamState();

    const size = extractSizeAlgorithm(strategy?.size);
    const highWaterMark = extractHighWaterMark(strategy?.highWaterMark, 1);

    setupWritableStreamDefaultControllerFromSink(
      this,
      sink,
      highWaterMark,
      size);
  }

  get [kIsErrored]() {
    return this[kState].state === 'errored';
  }

  get [kIsWritable]() {
    return this[kState].state === 'writable';
  }

  [kControllerErrorFunction](error) {
    // Used by the internal stream interop (addAbortSignal).
    this[kState].controller.error(error);
  }

  // Used by the internal stream interop (end-of-stream). Materialized
  // lazily since its settlement is derivable from the stream state.
  get [kIsClosedPromise]() {
    let cache = this[kState].closedPromise;
    if (cache === undefined) {
      switch (this[kState].state) {
        case 'writable':
        case 'erroring':
          cache = PromiseWithResolvers();
          break;
        case 'closed':
          cache = resolvedRecord();
          break;
        default:
          cache = rejectedHandledRecord(this[kState].storedError);
      }
      this[kState].closedPromise = cache;
    }
    return cache;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get locked() {
    if (!isWritableStream(this))
      throw new ERR_INVALID_THIS('WritableStream');
    return isWritableStreamLocked(this);
  }

  /**
   * @param {any} [reason]
   * @returns {Promise<void>}
   */
  abort(reason = undefined) {
    if (!isWritableStream(this))
      return PromiseReject(new ERR_INVALID_THIS('WritableStream'));
    if (isWritableStreamLocked(this)) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError('WritableStream is locked'));
    }
    return writableStreamAbort(this, reason);
  }

  /**
   * @returns {Promise<void>}
   */
  close() {
    if (!isWritableStream(this))
      return PromiseReject(new ERR_INVALID_THIS('WritableStream'));
    if (isWritableStreamLocked(this)) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError('WritableStream is locked'));
    }
    if (writableStreamCloseQueuedOrInFlight(this)) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError('Failure closing WritableStream'));
    }
    return writableStreamClose(this);
  }

  /**
   * @returns {WritableStreamDefaultWriter}
   */
  getWriter() {
    if (!isWritableStream(this))
      throw new ERR_INVALID_THIS('WritableStream');
    // eslint-disable-next-line no-use-before-define
    return new WritableStreamDefaultWriter(this);
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      locked: this.locked,
      state: this[kState].state,
    });
  }

  [kTransfer]() {
    if (!isWritableStream(this))
      throw new ERR_INVALID_THIS('WritableStream');
    if (this.locked) {
      this[kState].transfer.port1?.close();
      this[kState].transfer.port1 = undefined;
      this[kState].transfer.port2 = undefined;
      throw new DOMException(
        'Cannot transfer a locked WritableStream',
        'DataCloneError');
    }

    const {
      readable,
      promise,
    } = lazyTransfer().newCrossRealmReadableStream(
      this,
      this[kState].transfer.port1);

    this[kState].transfer.readable = readable;
    this[kState].transfer.promise = promise;

    return {
      data: { port: this[kState].transfer.port2 },
      deserializeInfo:
        'internal/webstreams/writablestream:TransferredWritableStream',
    };
  }

  [kTransferList]() {
    const { port1, port2 } = new MessageChannel();
    this[kState].transfer.port1 = port1;
    this[kState].transfer.port2 = port2;
    return [ port2 ];
  }

  [kDeserialize]({ port }) {
    const transfer = lazyTransfer();
    setupWritableStreamDefaultControllerFromSink(
      this,
      // The MessagePort is set to be referenced when reading.
      // After two MessagePorts are closed, there is a problem with
      // lingering promise not being properly resolved.
      // https://github.com/nodejs/node/issues/51486
      new transfer.CrossRealmTransformWritableSink(port, true),
      1,
      defaultSizeAlgorithm);
  }
}

ObjectDefineProperties(WritableStream.prototype, {
  locked: kEnumerableProperty,
  abort: kEnumerableProperty,
  close: kEnumerableProperty,
  getWriter: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(WritableStream.name),
});

function InternalTransferredWritableStream() {
  ObjectSetPrototypeOf(this, WritableStream.prototype);
  markTransferMode(this, false, true);
  this[kType] = 'WritableStream';
  this[kState] = createWritableStreamState();
}

ObjectSetPrototypeOf(InternalTransferredWritableStream.prototype, WritableStream.prototype);
ObjectSetPrototypeOf(InternalTransferredWritableStream, WritableStream);

function TransferredWritableStream() {
  const stream = new InternalTransferredWritableStream();

  stream.constructor = WritableStream;

  return stream;
}

TransferredWritableStream.prototype[kDeserialize] = () => {};

class WritableStreamDefaultWriter {
  [kType] = 'WritableStreamDefaultWriter';

  /**
   * @param {WritableStream} stream
   */
  constructor(stream) {
    if (!isWritableStream(stream))
      throw new ERR_INVALID_ARG_TYPE('stream', 'WritableStream', stream);
    this[kState] = {
      stream: undefined,
      // Lazily materialized by writerClosedPromise()/writerReadyPromise().
      close: undefined,
      ready: undefined,
    };
    setupWritableStreamDefaultWriter(this, stream);
  }

  /**
   * @readonly
   * @type {Promise<void>}
   */
  get closed() {
    if (!isWritableStreamDefaultWriter(this))
      return PromiseReject(new ERR_INVALID_THIS('WritableStreamDefaultWriter'));
    return writerClosedPromise(this).promise;
  }

  /**
   * @readonly
   * @type {number}
   */
  get desiredSize() {
    if (!isWritableStreamDefaultWriter(this))
      throw new ERR_INVALID_THIS('WritableStreamDefaultWriter');
    if (this[kState].stream === undefined) {
      throw new ERR_INVALID_STATE.TypeError(
        'Writer is not bound to a WritableStream');
    }
    return writableStreamDefaultWriterGetDesiredSize(this);
  }

  /**
   * @readonly
   * @type {Promise<void>}
   */
  get ready() {
    if (!isWritableStreamDefaultWriter(this))
      return PromiseReject(new ERR_INVALID_THIS('WritableStreamDefaultWriter'));
    return writerReadyPromise(this).promise;
  }

  /**
   * @param {any} [reason]
   * @returns {Promise<void>}
   */
  abort(reason = undefined) {
    if (!isWritableStreamDefaultWriter(this))
      return PromiseReject(new ERR_INVALID_THIS('WritableStreamDefaultWriter'));
    if (this[kState].stream === undefined) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError(
          'Writer is not bound to a WritableStream'));
    }
    return writableStreamDefaultWriterAbort(this, reason);
  }

  /**
   * @returns {Promise<void>}
   */
  close() {
    if (!isWritableStreamDefaultWriter(this))
      return PromiseReject(new ERR_INVALID_THIS('WritableStreamDefaultWriter'));
    const {
      stream,
    } = this[kState];
    if (stream === undefined) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError(
          'Writer is not bound to a WritableStream'));
    }
    if (writableStreamCloseQueuedOrInFlight(stream)) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError('Failure to close WritableStream'));
    }
    return writableStreamDefaultWriterClose(this);
  }

  releaseLock() {
    if (!isWritableStreamDefaultWriter(this))
      throw new ERR_INVALID_THIS('WritableStreamDefaultWriter');
    const {
      stream,
    } = this[kState];
    if (stream === undefined)
      return;
    assert(stream[kState].writer !== undefined);
    writableStreamDefaultWriterRelease(this);
  }

  /**
   * @param {any} [chunk]
   * @returns {Promise<void>}
   */
  write(chunk = undefined) {
    if (!isWritableStreamDefaultWriter(this))
      return PromiseReject(new ERR_INVALID_THIS('WritableStreamDefaultWriter'));
    if (this[kState].stream === undefined) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError(
          'Writer is not bound to a WritableStream'));
    }
    return writableStreamDefaultWriterWrite(this, chunk);
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      stream: this[kState].stream,
      close: writerClosedPromise(this).promise,
      ready: writerReadyPromise(this).promise,
      desiredSize: this.desiredSize,
    });
  }
}

ObjectDefineProperties(WritableStreamDefaultWriter.prototype, {
  closed: kEnumerableProperty,
  ready: kEnumerableProperty,
  desiredSize: kEnumerableProperty,
  abort: kEnumerableProperty,
  close: kEnumerableProperty,
  releaseLock: kEnumerableProperty,
  write: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(WritableStreamDefaultWriter.name),
});

class WritableStreamDefaultController {
  [kType] = 'WritableStreamDefaultController';

  constructor(skipThrowSymbol = undefined) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }

  [kAbort](reason) {
    const result = this[kState].abortAlgorithm(reason);
    writableStreamDefaultControllerClearAlgorithms(this);
    return result;
  }

  [kError]() {
    resetQueue(this);
  }

  /**
   * @type {AbortSignal}
   */
  get signal() {
    if (!isWritableStreamDefaultController(this))
      throw new ERR_INVALID_THIS('WritableStreamDefaultController');
    return this[kState].abortController.signal;
  }

  /**
   * @param {any} [error]
   */
  error(error = undefined) {
    if (!isWritableStreamDefaultController(this))
      throw new ERR_INVALID_THIS('WritableStreamDefaultController');
    if (this[kState].stream[kState].state !== 'writable')
      return;
    writableStreamDefaultControllerError(this, error);
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      stream: this[kState].stream,
    });
  }
}

ObjectDefineProperties(WritableStreamDefaultController.prototype, {
  signal: kEnumerableProperty,
  error: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(WritableStreamDefaultController.name),
});

function InternalWritableStream(start, write, close, abort, highWaterMark, size) {
  ObjectSetPrototypeOf(this, WritableStream.prototype);
  markTransferMode(this, false, true);
  this[kType] = 'WritableStream';
  this[kState] = createWritableStreamState();

  const controller = new WritableStreamDefaultController(kSkipThrow);
  setupWritableStreamDefaultController(
    this,
    controller,
    start,
    write,
    close,
    abort,
    highWaterMark,
    size,
  );
}

ObjectSetPrototypeOf(InternalWritableStream.prototype, WritableStream.prototype);
ObjectSetPrototypeOf(InternalWritableStream, WritableStream);

function createWritableStream(start, write, close, abort, highWaterMark = 1, size = defaultSizeAlgorithm) {
  const stream = new InternalWritableStream(start, write, close, abort, highWaterMark, size);

  // For spec compliance the InternalWritableStream must be a WritableStream
  stream.constructor = WritableStream;
  return stream;
}

const isWritableStream =
  isBrandCheck('WritableStream');
const isWritableStreamDefaultWriter =
  isBrandCheck('WritableStreamDefaultWriter');
const isWritableStreamDefaultController =
  isBrandCheck('WritableStreamDefaultController');

function createWritableStreamState() {
  return {
    __proto__: null,
    closedPromise: undefined,
    closeRequest: kNilRequest,
    // Mirrors "closeRequest or inFlightCloseRequest is pending"; kept as a
    // flag because the predicate runs several times per chunk on the write
    // hot path.
    closeQueuedOrInFlight: false,
    inFlightWriteRequest: kNilRequest,
    inFlightCloseRequest: kNilRequest,
    pendingAbortRequest: kNilPendingAbortRequest,
    backpressure: false,
    controller: undefined,
    state: 'writable',
    storedError: undefined,
    // Ring-buffer request queue, materialized lazily on the first pending
    // write (see writableStreamAddWriteRequest) so construction allocates
    // no request storage.
    writeRequests: kEmptyQueue,
    writer: undefined,
    transfer: {
      __proto__: null,
      readable: undefined,
      port1: undefined,
      port2: undefined,
      promise: undefined,
    },
  };
}

function isWritableStreamLocked(stream) {
  return stream[kState].writer !== undefined;
}

function setupWritableStreamDefaultWriter(writer, stream) {
  if (isWritableStreamLocked(stream))
    throw new ERR_INVALID_STATE.TypeError('WritableStream is locked');
  writer[kState].stream = stream;
  stream[kState].writer = writer;
  // The writer's [[readyPromise]] and [[closedPromise]] are materialized
  // lazily by writerReadyPromise()/writerClosedPromise(); their settlement
  // is fully derivable from the writer/stream state, so nothing is
  // allocated here.
}

// Materializes the writer's [[closedPromise]] record on first
// observation. While unobserved, its settlement is derivable: a detached
// writer was released (rejected with the shared released error);
// otherwise it follows the stream state. The settle sites only touch the
// record when it has been materialized.
function writerClosedPromise(writer) {
  let close = writer[kState].close;
  if (close === undefined) {
    const stream = writer[kState].stream;
    if (stream === undefined) {
      close = rejectedHandledRecord(lazyWritableReleasedError());
    } else {
      switch (stream[kState].state) {
        case 'writable':
        case 'erroring':
          close = PromiseWithResolvers();
          break;
        case 'closed':
          close = resolvedRecord();
          break;
        default:
          close = rejectedHandledRecord(stream[kState].storedError);
      }
    }
    writer[kState].close = close;
  }
  return close;
}

// Same as writerClosedPromise(), for the writer's [[readyPromise]]. The
// pending epoch is derivable too: a writable stream with backpressure and
// no queued close has a pending ready promise.
function writerReadyPromise(writer) {
  let ready = writer[kState].ready;
  if (ready === undefined) {
    const stream = writer[kState].stream;
    if (stream === undefined) {
      ready = rejectedHandledRecord(lazyWritableReleasedError());
    } else {
      switch (stream[kState].state) {
        case 'writable':
          ready = !writableStreamCloseQueuedOrInFlight(stream) &&
                  stream[kState].backpressure ?
            PromiseWithResolvers() :
            resolvedRecord();
          break;
        case 'closed':
          ready = resolvedRecord();
          break;
        default:
          ready = rejectedHandledRecord(stream[kState].storedError);
      }
    }
    writer[kState].ready = ready;
  }
  return ready;
}

function writableStreamAbort(stream, reason) {
  const {
    state,
    controller,
  } = stream[kState];
  if (state === 'closed' || state === 'errored')
    return PromiseResolve();

  controller[kState].abortController.abort(reason);

  if (stream[kState].pendingAbortRequest.abort.promise !== undefined)
    return stream[kState].pendingAbortRequest.abort.promise;

  assert(state === 'writable' || state === 'erroring');

  let wasAlreadyErroring = false;
  if (state === 'erroring') {
    wasAlreadyErroring = true;
    reason = undefined;
  }

  const abort = PromiseWithResolvers();

  stream[kState].pendingAbortRequest = {
    abort,
    reason,
    wasAlreadyErroring,
  };

  if (!wasAlreadyErroring)
    writableStreamStartErroring(stream, reason);

  return abort.promise;
}

function writableStreamClose(stream) {
  const {
    state,
    writer,
    backpressure,
    controller,
  } = stream[kState];
  if (state === 'closed' || state === 'errored') {
    return PromiseReject(
      new ERR_INVALID_STATE.TypeError('WritableStream is closed'));
  }
  assert(state === 'writable' || state === 'erroring');
  assert(!writableStreamCloseQueuedOrInFlight(stream));
  stream[kState].closeRequest = PromiseWithResolvers();
  stream[kState].closeQueuedOrInFlight = true;
  const { promise } = stream[kState].closeRequest;
  if (writer !== undefined && backpressure && state === 'writable')
    writer[kState].ready?.resolve();
  writableStreamDefaultControllerClose(controller);
  return promise;
}

function writableStreamUpdateBackpressure(stream, backpressure) {
  const streamState = stream[kState];
  assert(streamState.state === 'writable');
  assert(!streamState.closeQueuedOrInFlight);
  const {
    writer,
  } = streamState;
  if (writer !== undefined && streamState.backpressure !== backpressure) {
    if (backpressure) {
      // The spec replaces [[readyPromise]] with a fresh pending promise;
      // dropping the cache lets the next observation derive it.
      writer[kState].ready = undefined;
    } else {
      writer[kState].ready?.resolve();
    }
  }
  streamState.backpressure = backpressure;
}

function writableStreamStartErroring(stream, reason) {
  assert(stream[kState].storedError === undefined);
  assert(stream[kState].state === 'writable');
  const {
    controller,
    writer,
  } = stream[kState];
  assert(controller !== undefined);
  stream[kState].state = 'erroring';
  stream[kState].storedError = reason;
  if (writer !== undefined) {
    writableStreamDefaultWriterEnsureReadyPromiseRejected(writer, reason);
  }
  if (!writableStreamHasOperationMarkedInFlight(stream) &&
      controller[kState].started) {
    writableStreamFinishErroring(stream);
  }
}

function writableStreamRejectCloseAndClosedPromiseIfNeeded(stream) {
  assert(stream[kState].state === 'errored');
  if (stream[kState].closeRequest.promise !== undefined) {
    assert(stream[kState].inFlightCloseRequest.promise === undefined);
    stream[kState].closeRequest.reject(stream[kState].storedError);
    stream[kState].closeRequest = kNilRequest;
    stream[kState].closeQueuedOrInFlight = false;
  }

  const closedPromiseCache = stream[kState].closedPromise;
  if (closedPromiseCache !== undefined) {
    setPromiseHandled(closedPromiseCache.promise);
    closedPromiseCache.reject(stream[kState].storedError);
  }

  const {
    writer,
  } = stream[kState];
  if (writer !== undefined) {
    const closeCache = writer[kState].close;
    if (closeCache !== undefined) {
      setPromiseHandled(closeCache.promise);
      closeCache.reject(stream[kState].storedError);
    }
  }
}

function writableStreamMarkFirstWriteRequestInFlight(stream) {
  assert(stream[kState].inFlightWriteRequest.promise === undefined);
  assert(stream[kState].writeRequests.length);
  const writeRequest = stream[kState].writeRequests.shift();
  stream[kState].inFlightWriteRequest = writeRequest;
}

function writableStreamMarkCloseRequestInFlight(stream) {
  assert(stream[kState].inFlightWriteRequest.promise === undefined);
  assert(stream[kState].closeRequest.promise !== undefined);
  stream[kState].inFlightCloseRequest = stream[kState].closeRequest;
  stream[kState].closeRequest = kNilRequest;
}

function writableStreamHasOperationMarkedInFlight(stream) {
  const {
    inFlightWriteRequest,
    inFlightCloseRequest,
  } = stream[kState];
  if (inFlightWriteRequest.promise === undefined &&
      inFlightCloseRequest.promise === undefined) {
    return false;
  }
  return true;
}

function writableStreamFinishInFlightWriteWithError(stream, error) {
  assert(stream[kState].inFlightWriteRequest.promise !== undefined);
  stream[kState].inFlightWriteRequest.reject(error);
  stream[kState].inFlightWriteRequest = kNilRequest;
  assert(stream[kState].state === 'writable' ||
         stream[kState].state === 'erroring');
  writableStreamDealWithRejection(stream, error);
}

function writableStreamFinishInFlightWrite(stream) {
  assert(stream[kState].inFlightWriteRequest.promise !== undefined);
  stream[kState].inFlightWriteRequest.resolve();
  stream[kState].inFlightWriteRequest = kNilRequest;
}

function writableStreamFinishInFlightCloseWithError(stream, error) {
  assert(stream[kState].inFlightCloseRequest.promise !== undefined);
  stream[kState].inFlightCloseRequest.reject(error);
  stream[kState].inFlightCloseRequest = kNilRequest;
  stream[kState].closeQueuedOrInFlight = false;
  assert(stream[kState].state === 'writable' ||
         stream[kState].state === 'erroring');
  if (stream[kState].pendingAbortRequest.abort.promise !== undefined) {
    stream[kState].pendingAbortRequest.abort.reject(error);
    stream[kState].pendingAbortRequest = kNilPendingAbortRequest;
  }
  writableStreamDealWithRejection(stream, error);
}

function writableStreamFinishInFlightClose(stream) {
  assert(stream[kState].inFlightCloseRequest.promise !== undefined);
  stream[kState].inFlightCloseRequest.resolve();
  stream[kState].inFlightCloseRequest = kNilRequest;
  stream[kState].closeQueuedOrInFlight = false;
  if (stream[kState].state === 'erroring') {
    stream[kState].storedError = undefined;
    if (stream[kState].pendingAbortRequest.abort.promise !== undefined) {
      stream[kState].pendingAbortRequest.abort.resolve();
      stream[kState].pendingAbortRequest = kNilPendingAbortRequest;
    }
  }
  stream[kState].state = 'closed';
  if (stream[kState].writer !== undefined)
    stream[kState].writer[kState].close?.resolve();
  stream[kState].closedPromise?.resolve();
  assert(stream[kState].pendingAbortRequest.abort.promise === undefined);
  assert(stream[kState].storedError === undefined);
}

function writableStreamFinishErroring(stream) {
  assert(stream[kState].state === 'erroring');
  assert(!writableStreamHasOperationMarkedInFlight(stream));
  stream[kState].state = 'errored';
  stream[kState].controller[kError]();
  const storedError = stream[kState].storedError;
  const writeRequests = stream[kState].writeRequests;
  while (writeRequests.length)
    writeRequests.shift().reject(storedError);

  if (stream[kState].pendingAbortRequest.abort.promise === undefined) {
    writableStreamRejectCloseAndClosedPromiseIfNeeded(stream);
    return;
  }

  const abortRequest = stream[kState].pendingAbortRequest;
  stream[kState].pendingAbortRequest = kNilPendingAbortRequest;
  if (abortRequest.wasAlreadyErroring) {
    abortRequest.abort.reject(storedError);
    writableStreamRejectCloseAndClosedPromiseIfNeeded(stream);
    return;
  }
  PromisePrototypeThen(
    stream[kState].controller[kAbort](abortRequest.reason),
    () => {
      abortRequest.abort.resolve();
      writableStreamRejectCloseAndClosedPromiseIfNeeded(stream);
    },
    (error) => {
      abortRequest.abort.reject(error);
      writableStreamRejectCloseAndClosedPromiseIfNeeded(stream);
    });
}

function writableStreamDealWithRejection(stream, error) {
  const {
    state,
  } = stream[kState];
  if (state === 'writable') {
    writableStreamStartErroring(stream, error);
    return;
  }

  assert(state === 'erroring');
  writableStreamFinishErroring(stream);
}

function writableStreamCloseQueuedOrInFlight(stream) {
  return stream[kState].closeQueuedOrInFlight;
}

function writableStreamAddWriteRequest(stream) {
  assert(isWritableStreamLocked(stream));
  assert(stream[kState].state === 'writable');
  // PromiseWithResolvers() already returns a { promise, resolve, reject }
  // record, so push it as-is instead of rebuilding an identical object.
  const writeRequest = PromiseWithResolvers();
  const streamState = stream[kState];
  let writeRequests = streamState.writeRequests;
  if (writeRequests === kEmptyQueue)
    writeRequests = streamState.writeRequests = new Queue();
  writeRequests.push(writeRequest);
  return writeRequest.promise;
}

function writableStreamDefaultWriterWrite(writer, chunk) {
  const writerState = writer[kState];
  const stream = writerState.stream;
  assert(stream !== undefined);
  const streamState = stream[kState];
  const {
    controller,
  } = streamState;
  const chunkSize = writableStreamDefaultControllerGetChunkSize(
    controller,
    chunk);
  if (stream !== writerState.stream) {
    return PromiseReject(
      new ERR_INVALID_STATE.TypeError('Mismatched WritableStreams'));
  }
  const {
    state,
  } = streamState;

  if (state === 'errored')
    return PromiseReject(streamState.storedError);

  if (streamState.closeQueuedOrInFlight || state === 'closed') {
    return PromiseReject(
      new ERR_INVALID_STATE.TypeError('WritableStream is closed'));
  }

  if (state === 'erroring')
    return PromiseReject(streamState.storedError);

  assert(state === 'writable');

  const promise = writableStreamAddWriteRequest(stream);
  writableStreamDefaultControllerWrite(controller, chunk, chunkSize);
  return promise;
}

function writableStreamDefaultWriterRelease(writer) {
  const {
    stream,
  } = writer[kState];
  assert(stream !== undefined);
  assert(stream[kState].writer === writer);
  const releasedStateError = lazyWritableReleasedError();
  writableStreamDefaultWriterEnsureReadyPromiseRejected(writer, releasedStateError);
  writableStreamDefaultWriterEnsureClosedPromiseRejected(writer, releasedStateError);
  stream[kState].writer = undefined;
  writer[kState].stream = undefined;
}

function writableStreamDefaultWriterGetDesiredSize(writer) {
  const {
    stream,
  } = writer[kState];
  switch (stream[kState].state) {
    case 'errored':
      // Fall through
    case 'erroring':
      return null;
    case 'closed':
      return 0;
  }
  return writableStreamDefaultControllerGetDesiredSize(
    stream[kState].controller);
}

function writableStreamDefaultWriterEnsureReadyPromiseRejected(writer, error) {
  const ready = writer[kState].ready;
  if (ready !== undefined && isPromisePending(ready.promise)) {
    ready.reject(error);
    setPromiseHandled(ready.promise);
  } else {
    // The spec replaces [[readyPromise]] with a promise rejected with the
    // same error the post-release/post-erroring state derives; dropping
    // the cache lets the next observation materialize it.
    writer[kState].ready = undefined;
  }
}

function writableStreamDefaultWriterEnsureClosedPromiseRejected(writer, error) {
  const close = writer[kState].close;
  if (close !== undefined && isPromisePending(close.promise)) {
    close.reject(error);
    setPromiseHandled(close.promise);
  } else {
    // See writableStreamDefaultWriterEnsureReadyPromiseRejected.
    writer[kState].close = undefined;
  }
}

function writableStreamDefaultWriterCloseWithErrorPropagation(writer) {
  const {
    stream,
  } = writer[kState];
  assert(stream !== undefined);
  const {
    state,
  } = stream[kState];
  if (writableStreamCloseQueuedOrInFlight(stream) || state === 'closed')
    return PromiseResolve();

  if (state === 'errored')
    return PromiseReject(stream[kState].storedError);

  assert(state === 'writable' || state === 'erroring');

  return writableStreamDefaultWriterClose(writer);
}

function writableStreamDefaultWriterClose(writer) {
  const {
    stream,
  } = writer[kState];
  assert(stream !== undefined);
  return writableStreamClose(stream);
}

function writableStreamDefaultWriterAbort(writer, reason) {
  const {
    stream,
  } = writer[kState];
  assert(stream !== undefined);
  return writableStreamAbort(stream, reason);
}

function writableStreamDefaultControllerWrite(controller, chunk, chunkSize) {
  try {
    enqueueValueWithSize(controller, chunk, chunkSize);
  } catch (error) {
    writableStreamDefaultControllerErrorIfNeeded(controller, error);
    return;
  }
  const {
    stream,
  } = controller[kState];
  const streamState = stream[kState];
  if (!streamState.closeQueuedOrInFlight &&
      streamState.state === 'writable') {
    writableStreamUpdateBackpressure(
      stream,
      writableStreamDefaultControllerGetBackpressure(controller));
  }
  writableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
}

function writableStreamDefaultControllerProcessWrite(controller, chunk) {
  const {
    stream,
    writeAlgorithm,
  } = controller[kState];
  writableStreamMarkFirstWriteRequestInFlight(stream);

  if (controller[kState].writeFulfilled === undefined) {
    // The write reaction closures only capture the stream and controller,
    // so they are created once on the first write and reused for every
    // subsequent write instead of allocating two fresh closures per chunk.
    controller[kState].writeFulfilled = () => {
      writableStreamFinishInFlightWrite(stream);
      const streamState = stream[kState];
      const {
        state,
      } = streamState;
      assert(state === 'writable' || state === 'erroring');
      dequeueValue(controller);
      if (!streamState.closeQueuedOrInFlight &&
          state === 'writable') {
        writableStreamUpdateBackpressure(
          stream,
          writableStreamDefaultControllerGetBackpressure(controller));
      }
      writableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
    };
    controller[kState].writeRejected = (error) => {
      if (stream[kState].state === 'writable')
        writableStreamDefaultControllerClearAlgorithms(controller);
      writableStreamFinishInFlightWriteWithError(stream, error);
    };
  }

  PromisePrototypeThen(
    writeAlgorithm(chunk, controller),
    controller[kState].writeFulfilled,
    controller[kState].writeRejected);
}

function writableStreamDefaultControllerProcessClose(controller) {
  const {
    closeAlgorithm,
    queue,
    stream,
  } = controller[kState];
  writableStreamMarkCloseRequestInFlight(stream);
  dequeueValue(controller);
  assert(!queue.length);
  const sinkClosePromise = closeAlgorithm();
  writableStreamDefaultControllerClearAlgorithms(controller);
  PromisePrototypeThen(
    sinkClosePromise,
    () => writableStreamFinishInFlightClose(stream),
    (error) => writableStreamFinishInFlightCloseWithError(stream, error));
}

function writableStreamDefaultControllerGetDesiredSize(controller) {
  const {
    highWaterMark,
    queueTotalSize,
  } = controller[kState];
  return highWaterMark - queueTotalSize;
}

function writableStreamDefaultControllerGetChunkSize(controller, chunk) {
  const {
    stream,
    sizeAlgorithm,
  } = controller[kState];
  if (sizeAlgorithm === undefined) {
    assert(stream[kState].state === 'closed' ||
           stream[kState].state === 'errored' ||
           stream[kState].state === 'erroring');
    return 1;
  }

  // The internal default size algorithm is never observable by user
  // code, always returns 1, and cannot throw: skip the call.
  if (sizeAlgorithm === defaultSizeAlgorithm)
    return 1;

  try {
    return FunctionPrototypeCall(
      sizeAlgorithm,
      undefined,
      chunk);
  } catch (error) {
    writableStreamDefaultControllerErrorIfNeeded(controller, error);
    return 1;
  }
}

function writableStreamDefaultControllerErrorIfNeeded(controller, error) {
  const {
    stream,
  } = controller[kState];
  if (stream[kState].state === 'writable')
    writableStreamDefaultControllerError(controller, error);
}

function writableStreamDefaultControllerError(controller, error) {
  const {
    stream,
  } = controller[kState];
  assert(stream[kState].state === 'writable');
  writableStreamDefaultControllerClearAlgorithms(controller);
  writableStreamStartErroring(stream, error);
}

function writableStreamDefaultControllerClose(controller) {
  enqueueValueWithSize(controller, kCloseSentinel, 0);
  writableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
}

function writableStreamDefaultControllerClearAlgorithms(controller) {
  controller[kState].writeAlgorithm = undefined;
  controller[kState].closeAlgorithm = undefined;
  controller[kState].abortAlgorithm = undefined;
  controller[kState].sizeAlgorithm = undefined;
}

function writableStreamDefaultControllerGetBackpressure(controller) {
  return writableStreamDefaultControllerGetDesiredSize(controller) <= 0;
}

function writableStreamDefaultControllerAdvanceQueueIfNeeded(controller) {
  const {
    queue,
    started,
    stream,
  } = controller[kState];
  if (!started || stream[kState].inFlightWriteRequest.promise !== undefined)
    return;

  if (stream[kState].state === 'erroring') {
    writableStreamFinishErroring(stream);
    return;
  }

  if (!queue.length)
    return;

  const value = peekQueueValue(controller);
  if (value === kCloseSentinel)
    writableStreamDefaultControllerProcessClose(controller);
  else
    writableStreamDefaultControllerProcessWrite(controller, value);
}

function setupWritableStreamDefaultControllerFromSink(
  stream,
  sink,
  highWaterMark,
  sizeAlgorithm) {
  const controller = new WritableStreamDefaultController(kSkipThrow);
  const start = sink?.start;
  const write = sink?.write;
  const close = sink?.close;
  const abort = sink?.abort;
  const startAlgorithm = start ?
    FunctionPrototypeBind(start, sink, controller) :
    nonOpStart;
  const writeAlgorithm = write ?
    createPromiseCallback2Params('sink.write', write, sink) :
    nonOpWrite;
  const closeAlgorithm = close ?
    createPromiseCallbackNoParams('sink.close', close, sink) :
    nonOpCancel;
  const abortAlgorithm = abort ?
    createPromiseCallback1Param('sink.abort', abort, sink) :
    nonOpCancel;
  setupWritableStreamDefaultController(
    stream,
    controller,
    startAlgorithm,
    writeAlgorithm,
    closeAlgorithm,
    abortAlgorithm,
    highWaterMark,
    sizeAlgorithm);
}

function setupWritableStreamDefaultController(
  stream,
  controller,
  startAlgorithm,
  writeAlgorithm,
  closeAlgorithm,
  abortAlgorithm,
  highWaterMark,
  sizeAlgorithm) {
  assert(isWritableStream(stream));
  assert(stream[kState].controller === undefined);
  controller[kState] = {
    abortAlgorithm,
    closeAlgorithm,
    highWaterMark,
    queue: kEmptyQueue,
    queueTotalSize: 0,
    abortController: new AbortController(),
    sizeAlgorithm,
    started: false,
    stream,
    writeAlgorithm,
    writeFulfilled: undefined,
    writeRejected: undefined,
  };
  stream[kState].controller = controller;

  writableStreamUpdateBackpressure(
    stream,
    writableStreamDefaultControllerGetBackpressure(controller));

  const startResult = startAlgorithm();

  if (startResult === null ||
      (typeof startResult !== 'object' && typeof startResult !== 'function')) {
    // Non-thenable start result: fulfillment is guaranteed and no .then
    // lookup on the result is observable, so run the post-start step
    // directly at the exact microtask position the promise reaction
    // would have had, skipping two promise allocations.
    queueMicrotask(() => {
      assert(stream[kState].state === 'writable' ||
             stream[kState].state === 'erroring');
      controller[kState].started = true;
      writableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
    });
    return;
  }

  PromisePrototypeThen(
    new Promise((r) => r(startResult)),
    () => {
      assert(stream[kState].state === 'writable' ||
             stream[kState].state === 'erroring');
      controller[kState].started = true;
      writableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
    },
    (error) => {
      assert(stream[kState].state === 'writable' ||
             stream[kState].state === 'erroring');
      controller[kState].started = true;
      writableStreamDealWithRejection(stream, error);
    });
}

module.exports = {
  WritableStream,
  WritableStreamDefaultWriter,
  WritableStreamDefaultController,
  TransferredWritableStream,

  // Exported Brand Checks
  isWritableStream,
  isWritableStreamDefaultController,
  isWritableStreamDefaultWriter,

  isWritableStreamLocked,
  setupWritableStreamDefaultWriter,
  writableStreamAbort,
  writableStreamClose,
  writableStreamUpdateBackpressure,
  writableStreamStartErroring,
  writableStreamRejectCloseAndClosedPromiseIfNeeded,
  writableStreamMarkFirstWriteRequestInFlight,
  writableStreamMarkCloseRequestInFlight,
  writableStreamHasOperationMarkedInFlight,
  writableStreamFinishInFlightWriteWithError,
  writableStreamFinishInFlightWrite,
  writableStreamFinishInFlightCloseWithError,
  writableStreamFinishInFlightClose,
  writableStreamFinishErroring,
  writableStreamDealWithRejection,
  writableStreamCloseQueuedOrInFlight,
  writableStreamAddWriteRequest,
  writableStreamDefaultWriterWrite,
  writableStreamDefaultWriterRelease,
  writableStreamDefaultWriterGetDesiredSize,
  writableStreamDefaultWriterEnsureReadyPromiseRejected,
  writableStreamDefaultWriterEnsureClosedPromiseRejected,
  writerClosedPromise,
  writerReadyPromise,
  writableStreamDefaultWriterCloseWithErrorPropagation,
  writableStreamDefaultWriterClose,
  writableStreamDefaultWriterAbort,
  writableStreamDefaultControllerWrite,
  writableStreamDefaultControllerProcessWrite,
  writableStreamDefaultControllerProcessClose,
  writableStreamDefaultControllerGetDesiredSize,
  writableStreamDefaultControllerGetChunkSize,
  writableStreamDefaultControllerErrorIfNeeded,
  writableStreamDefaultControllerError,
  writableStreamDefaultControllerClose,
  writableStreamDefaultControllerClearAlgorithms,
  writableStreamDefaultControllerGetBackpressure,
  writableStreamDefaultControllerAdvanceQueueIfNeeded,
  setupWritableStreamDefaultControllerFromSink,
  setupWritableStreamDefaultController,
  createWritableStream,
};
