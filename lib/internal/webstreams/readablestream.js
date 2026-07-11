'use strict';

const {
  ArrayBuffer,
  ArrayBufferPrototypeGetByteLength,
  ArrayBufferPrototypeGetDetached,
  ArrayBufferPrototypeSlice,
  ArrayBufferPrototypeTransfer,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  DataView,
  FunctionPrototypeBind,
  FunctionPrototypeCall,
  MathMin,
  NumberIsInteger,
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SafePromiseAll,
  Symbol,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
  SymbolToStringTag,
  TypedArrayPrototypeGetLength,
  Uint8Array,
} = primordials;

const {
  AbortError,
  codes: {
    ERR_ARG_NOT_ITERABLE,
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const {
  DOMException,
} = internalBinding('messaging');

const {
  markPromiseAsHandled,
} = internalBinding('util');

const {
  isArrayBufferView,
  isDataView,
  isSharedArrayBuffer,
} = require('internal/util/types');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
  kEnumerableProperty,
  SideEffectFreeRegExpPrototypeSymbolReplace,
} = require('internal/util');

const {
  validateAbortSignal,
  validateBuffer,
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
  structuredClone,
} = require('internal/worker/js_transferable');

const {
  queueMicrotask,
} = require('internal/process/task_queues');

const {
  kIsDisturbed,
  kIsErrored,
  kIsReadable,
  kIsClosedPromise,
  kControllerErrorFunction,
} = require('internal/streams/utils');

const {
  ArrayBufferViewGetBuffer,
  ArrayBufferViewGetByteLength,
  ArrayBufferViewGetByteOffset,
  AsyncIterator,
  canCopyArrayBuffer,
  cloneAsUint8Array,
  copyArrayBuffer,
  createPromiseCallback1Param,
  customInspect,
  defaultSizeAlgorithm,
  dequeueValue,
  enqueueValueWithSize,
  extractHighWaterMark,
  extractSizeAlgorithm,
  getNonWritablePropertyDescriptor,
  isBrandCheck,
  kEmptyQueue,
  kState,
  kType,
  lazyTransfer,
  materializeQueue,
  nonOpCancel,
  nonOpPull,
  nonOpStart,
  rejectedHandledRecord,
  resetQueue,
  resolvedRecord,
  setPromiseHandled,
} = require('internal/webstreams/util');

const {
  WritableStreamDefaultWriter,

  isWritableStream,
  isWritableStreamLocked,
  isWritableStreamDefaultController,
  isWritableStreamDefaultWriter,

  writableStreamAbort,
  writableStreamCloseQueuedOrInFlight,
  writableStreamDefaultWriterCloseWithErrorPropagation,
  writableStreamDefaultWriterRelease,
  writableStreamDefaultWriterWrite,
  writerClosedPromise,
  writerReadyPromise,
} = require('internal/webstreams/writablestream');

const { Buffer } = require('buffer');

const assert = require('internal/assert');

const kCancel = Symbol('kCancel');
const kClose = Symbol('kClose');
const kChunk = Symbol('kChunk');
const kError = Symbol('kError');
const kPull = Symbol('kPull');
const kRelease = Symbol('kRelease');
const kSkipThrow = Symbol('kSkipThrow');

let releasedError;
let releasingError;
let addAbortListener;

const userModuleRegExp = /^ {4}at (?:[^/\\(]+ \()(?!node:(.+):\d+:\d+\)$).*/gm;

function lazyReadableReleasedError() {
  if (releasedError) {
    return releasedError;
  }

  releasedError = new ERR_INVALID_STATE.TypeError('Reader released');
  // Avoid V8 leak and remove userland stackstrace
  releasedError.stack = SideEffectFreeRegExpPrototypeSymbolReplace(userModuleRegExp, releasedError.stack, '');
  return releasedError;
}

function lazyReadableReleasingError() {
  if (releasingError) {
    return releasingError;
  }
  releasingError = new ERR_INVALID_STATE.TypeError('Releasing reader');
  // Avoid V8 leak and remove userland stackstrace
  releasingError.stack = SideEffectFreeRegExpPrototypeSymbolReplace(userModuleRegExp, releasingError.stack, '');
  return releasingError;
}

/**
 * @typedef {import('../abort_controller').AbortSignal} AbortSignal
 * @typedef {import('./queuingstrategies').QueuingStrategy} QueuingStrategy
 * @typedef {import('./queuingstrategies').QueuingStrategySize
 * } QueuingStrategySize
 * @typedef {import('./writablestream').WritableStream} WritableStream
 */

/**
 * @typedef {ReadableStreamDefaultController | ReadableByteStreamController
 * } ReadableStreamController
 */

/**
 * @typedef {ReadableStreamDefaultReader | ReadableStreamBYOBReader
 * } ReadableStreamReader
 */

/**
 * @callback UnderlyingSourceStartCallback
 * @param {ReadableStreamController} controller
 * @returns { any | Promise<void> }
 */

/**
 * @callback UnderlyingSourcePullCallback
 * @param {ReadableStreamController} controller
 * @returns { Promise<void> }
 */

/**
 * @callback UnderlyingSourceCancelCallback
 * @param {any} reason
 * @returns { Promise<void> }
 */

/**
 * @typedef {{
 *   readable: ReadableStream,
 *   writable: WritableStream,
 * }} ReadableWritablePair
 */

/**
 * @typedef {{
 *   preventClose? : boolean,
 *   preventAbort? : boolean,
 *   preventCancel? : boolean,
 *   signal? : AbortSignal,
 * }} StreamPipeOptions
 */

/**
 * @typedef {{
 *   start? : UnderlyingSourceStartCallback,
 *   pull? : UnderlyingSourcePullCallback,
 *   cancel? : UnderlyingSourceCancelCallback,
 *   type? : "bytes",
 *   autoAllocateChunkSize? : number
 * }} UnderlyingSource
 */

class ReadableStream {
  [kType] = 'ReadableStream';

  /**
   * @param {UnderlyingSource} [source]
   * @param {QueuingStrategy} [strategy]
   */
  constructor(source = kEmptyObject, strategy = kEmptyObject) {
    markTransferMode(this, false, true);
    validateObject(source, 'source', kValidateObjectAllowObjects);
    validateObject(strategy, 'strategy', kValidateObjectAllowObjectsAndNull);
    this[kState] = createReadableStreamState();

    // The spec requires handling of the strategy first
    // here. Specifically, if getting the size and
    // highWaterMark from the strategy fail, that has
    // to trigger a throw before getting the details
    // from the source. So be sure to keep these in
    // this order.
    const size = strategy?.size;
    const highWaterMark = strategy?.highWaterMark;
    const type = source.type;

    if (`${type}` === 'bytes') {
      if (size !== undefined)
        throw new ERR_INVALID_ARG_VALUE.RangeError('strategy.size', size);
      setupReadableByteStreamControllerFromSource(
        this,
        source,
        extractHighWaterMark(highWaterMark, 0));
    } else {
      if (type !== undefined)
        throw new ERR_INVALID_ARG_VALUE('source.type', type);
      setupReadableStreamDefaultControllerFromSource(
        this,
        source,
        extractHighWaterMark(highWaterMark, 1),
        extractSizeAlgorithm(size));
    }
  }

  get [kIsDisturbed]() {
    return this[kState].disturbed;
  }

  get [kIsErrored]() {
    return this[kState].state === 'errored';
  }

  get [kIsReadable]() {
    return this[kState].state === 'readable';
  }

  [kControllerErrorFunction](error) {
    // Used by the internal stream interop (addAbortSignal). Historically
    // only default controllers were wired here; byte stream controllers
    // keep the previous no-op behavior.
    const controller = this[kState].controller;
    if (isReadableStreamDefaultController(controller))
      controller.error(error);
  }

  // Used by the internal stream interop (end-of-stream). Materialized
  // lazily since its settlement is derivable from the stream state; the
  // settle sites in readableStreamClose/Error only touch the cache.
  get [kIsClosedPromise]() {
    let cache = this[kState].closedPromise;
    if (cache === undefined) {
      switch (this[kState].state) {
        case 'readable':
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
    if (!isReadableStream(this))
      throw new ERR_INVALID_THIS('ReadableStream');
    return isReadableStreamLocked(this);
  }

  static from(iterable) {
    return readableStreamFromIterable(iterable);
  }

  /**
   * @param {any} [reason]
   * @returns { Promise<void> }
   */
  cancel(reason = undefined) {
    if (!isReadableStream(this))
      return PromiseReject(new ERR_INVALID_THIS('ReadableStream'));
    if (isReadableStreamLocked(this)) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError('ReadableStream is locked'));
    }
    return readableStreamCancel(this, reason);
  }

  /**
   * @param {{
   *   mode? : "byob"
   * }} [options]
   * @returns {ReadableStreamReader}
   */
  getReader(options = kEmptyObject) {
    if (!isReadableStream(this))
      throw new ERR_INVALID_THIS('ReadableStream');
    validateObject(options, 'options', kValidateObjectAllowObjectsAndNull);
    const mode = options?.mode;

    if (mode === undefined)
      // eslint-disable-next-line no-use-before-define
      return new ReadableStreamDefaultReader(this);

    if (`${mode}` !== 'byob')
      throw new ERR_INVALID_ARG_VALUE('options.mode', mode);
    // eslint-disable-next-line no-use-before-define
    return new ReadableStreamBYOBReader(this);
  }

  /**
   * @param {ReadableWritablePair} transform
   * @param {StreamPipeOptions} [options]
   * @returns {ReadableStream}
   */
  pipeThrough(transform, options = kEmptyObject) {
    if (!isReadableStream(this))
      throw new ERR_INVALID_THIS('ReadableStream');
    const readable = transform?.readable;
    if (!isReadableStream(readable)) {
      throw new ERR_INVALID_ARG_TYPE(
        'transform.readable',
        'ReadableStream',
        readable);
    }
    const writable = transform?.writable;
    if (!isWritableStream(writable)) {
      throw new ERR_INVALID_ARG_TYPE(
        'transform.writable',
        'WritableStream',
        writable);
    }

    // The web platform tests require that these be handled one at a
    // time and in a specific order. options can be null or undefined.
    validateObject(options, 'options', kValidateObjectAllowObjectsAndNull);
    const preventAbort = options?.preventAbort;
    const preventCancel = options?.preventCancel;
    const preventClose = options?.preventClose;
    const signal = options?.signal;

    if (signal !== undefined) {
      validateAbortSignal(signal, 'options.signal');
    }

    if (isReadableStreamLocked(this))
      throw new ERR_INVALID_STATE.TypeError('The ReadableStream is locked');
    if (isWritableStreamLocked(writable))
      throw new ERR_INVALID_STATE.TypeError('The WritableStream is locked');

    const promise = readableStreamPipeTo(
      this,
      writable,
      !!preventClose,
      !!preventAbort,
      !!preventCancel,
      signal);
    setPromiseHandled(promise);

    return readable;
  }

  /**
   * @param {WritableStream} destination
   * @param {StreamPipeOptions} [options]
   * @returns {Promise<void>}
   */
  pipeTo(destination, options = kEmptyObject) {
    try {
      if (!isReadableStream(this))
        throw new ERR_INVALID_THIS('ReadableStream');
      if (!isWritableStream(destination)) {
        throw new ERR_INVALID_ARG_TYPE(
          'transform.writable',
          'WritableStream',
          destination);
      }

      validateObject(options, 'options', kValidateObjectAllowObjectsAndNull);
      const preventAbort = options?.preventAbort;
      const preventCancel = options?.preventCancel;
      const preventClose = options?.preventClose;
      const signal = options?.signal;

      if (signal !== undefined) {
        validateAbortSignal(signal, 'options.signal');
      }

      if (isReadableStreamLocked(this))
        throw new ERR_INVALID_STATE.TypeError('The ReadableStream is locked');
      if (isWritableStreamLocked(destination))
        throw new ERR_INVALID_STATE.TypeError('The WritableStream is locked');

      return readableStreamPipeTo(
        this,
        destination,
        !!preventClose,
        !!preventAbort,
        !!preventCancel,
        signal);
    } catch (error) {
      return PromiseReject(error);
    }
  }

  /**
   * @returns {ReadableStream[]}
   */
  tee() {
    if (!isReadableStream(this))
      throw new ERR_INVALID_THIS('ReadableStream');
    return readableStreamTee(this, false);
  }

  /**
   * @param {{
   *   preventCancel? : boolean,
   * }} [options]
   * @returns {AsyncIterable}
   */
  values(options = kEmptyObject) {
    if (!isReadableStream(this))
      throw new ERR_INVALID_THIS('ReadableStream');
    validateObject(options, 'options', kValidateObjectAllowObjectsAndNull);
    const preventCancel = !!(options?.preventCancel);

    // eslint-disable-next-line no-use-before-define
    const reader = new ReadableStreamDefaultReader(this);

    // No __proto__ here to avoid the performance hit.
    const state = {
      done: false,
      current: undefined,
    };
    let started = false;
    // A single reusable read request: at most one read is ever in flight
    // (next() chains through state.current), and the request is consumed
    // before the next read starts, so only its promise record changes
    // per read.
    // eslint-disable-next-line no-use-before-define
    const readRequest = new ReadableStreamAsyncIteratorReadRequest(reader, state, undefined);

    // The nextSteps function is not an async function in order
    // to make it more efficient. Because nextSteps explicitly
    // creates a Promise and returns it in the common case,
    // making it an async function just causes two additional
    // unnecessary Promise allocations to occur, which just add
    // cost.
    function nextSteps() {
      if (state.done)
        return PromiseResolve({ done: true, value: undefined });

      if (reader[kState].stream === undefined) {
        return PromiseReject(
          new ERR_INVALID_STATE.TypeError(
            'The reader is not bound to a ReadableStream'));
      }
      const promise = PromiseWithResolvers();

      readRequest.promise = promise;
      readableStreamDefaultReaderRead(reader, readRequest);
      return promise.promise;
    }

    async function returnSteps(value) {
      if (state.done)
        return { done: true, value }; // eslint-disable-line node-core/avoid-prototype-pollution
      state.done = true;

      if (reader[kState].stream === undefined) {
        throw new ERR_INVALID_STATE.TypeError(
          'The reader is not bound to a ReadableStream');
      }
      assert(!reader[kState].readRequests.length);
      if (!preventCancel) {
        const result = readableStreamReaderGenericCancel(reader, value);
        readableStreamReaderGenericRelease(reader);
        await result;
        return { done: true, value }; // eslint-disable-line node-core/avoid-prototype-pollution
      }

      readableStreamReaderGenericRelease(reader);
      return { done: true, value }; // eslint-disable-line node-core/avoid-prototype-pollution
    }

    // TODO(@jasnell): Explore whether an async generator
    // can be used here instead of a custom iterator object.
    return ObjectSetPrototypeOf({
      // Changing either of these functions (next or return)
      // to async functions causes a failure in the streams
      // Web Platform Tests that check for use of a modified
      // Promise.prototype.then. Since the await keyword
      // uses Promise.prototype.then, it is open to prototype
      // pollution, which causes the test to fail. The other
      // await uses here do not trigger that failure because
      // the test that fails does not trigger those code paths.
      next() {
        // If this is the first read, delay by one microtask
        // to ensure that the controller has had an opportunity
        // to properly start and perform the initial pull.
        // TODO(@jasnell): The spec doesn't call this out so
        // need to investigate if it's a bug in our impl or
        // the spec.
        if (!started) {
          state.current = PromiseResolve();
          started = true;
        }
        if (state.current !== undefined) {
          state.current =
            PromisePrototypeThen(state.current, nextSteps, nextSteps);
          return state.current;
        }
        // No read is in flight. Mirror the buffered fast path of
        // ReadableStreamDefaultReader.read(): when data is already queued
        // in the controller, resolve immediately without allocating a
        // read request. The result settles synchronously, so leaving
        // state.current undefined matches the state the slow path reaches
        // once its read request callbacks have settled.
        const stream = reader[kState].stream;
        if (!state.done && stream !== undefined &&
            stream[kState].state === 'readable') {
          const controller = stream[kState].controller;
          if (isReadableStreamDefaultController(controller)) {
            if (controller[kState].queue.length > 0) {
              stream[kState].disturbed = true;
              const chunk = dequeueValue(controller);

              if (controller[kState].closeRequested &&
                  !controller[kState].queue.length) {
                readableStreamDefaultControllerClearAlgorithms(controller);
                readableStreamClose(stream);
              } else if (!controller[kState].closeRequested &&
                         controller[kState].started &&
                         controller[kState].highWaterMark -
                           controller[kState].queueTotalSize > 0) {
                // Reduced ShouldCallPull, as in the read() fast path.
                readableStreamDefaultControllerPull(controller);
              }

              return PromiseResolve({ done: false, value: chunk });
            }
          } else if (controller[kState].queueTotalSize > 0) {
            // Byte controller with buffered data: same shape as above via
            // the queue-filled arm of the byte controller's pull steps.
            stream[kState].disturbed = true;
            return PromiseResolve({
              done: false,

              value: readableByteStreamControllerDequeueChunk(controller),
            });
          }
        }
        state.current = nextSteps();
        return state.current;
      },

      return(error) {
        started = true;
        state.current = state.current !== undefined ?
          PromisePrototypeThen(
            state.current,
            () => returnSteps(error),
            () => returnSteps(error)) :
          returnSteps(error);
        return state.current;
      },

      [SymbolAsyncIterator]() { return this; },
    }, AsyncIterator);
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      locked: this.locked,
      state: this[kState].state,
      supportsBYOB:
        // eslint-disable-next-line no-use-before-define
        this[kState].controller instanceof ReadableByteStreamController,
    });
  }

  [kTransfer]() {
    if (!isReadableStream(this))
      throw new ERR_INVALID_THIS('ReadableStream');
    if (this.locked) {
      this[kState].transfer.port1?.close();
      this[kState].transfer.port1 = undefined;
      this[kState].transfer.port2 = undefined;
      throw new DOMException(
        'Cannot transfer a locked ReadableStream',
        'DataCloneError');
    }

    const {
      writable,
      promise,
    } = lazyTransfer().newCrossRealmWritableSink(
      this,
      this[kState].transfer.port1);

    this[kState].transfer.writable = writable;
    this[kState].transfer.promise = promise;

    return {
      data: { port: this[kState].transfer.port2 },
      deserializeInfo:
        'internal/webstreams/readablestream:TransferredReadableStream',
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
    setupReadableStreamDefaultControllerFromSource(
      this,
      // The MessagePort is set to be referenced when reading.
      // After two MessagePorts are closed, there is a problem with
      // lingering promise not being properly resolved.
      // https://github.com/nodejs/node/issues/51486
      new transfer.CrossRealmTransformReadableSource(port, true),
      0, defaultSizeAlgorithm);
  }
}

ObjectDefineProperties(ReadableStream.prototype, {
  [SymbolAsyncIterator]: {
    __proto__: null,
    configurable: true,
    enumerable: false,
    writable: true,
    value: ReadableStream.prototype.values,
  },
  locked: kEnumerableProperty,
  cancel: kEnumerableProperty,
  getReader: kEnumerableProperty,
  pipeThrough: kEnumerableProperty,
  pipeTo: kEnumerableProperty,
  tee: kEnumerableProperty,
  values: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(ReadableStream.name),
});
ObjectDefineProperties(ReadableStream, {
  from: kEnumerableProperty,
});

function InternalTransferredReadableStream() {
  ObjectSetPrototypeOf(this, ReadableStream.prototype);
  markTransferMode(this, false, true);
  this[kType] = 'ReadableStream';
  this[kState] = createReadableStreamState();
}

ObjectSetPrototypeOf(InternalTransferredReadableStream.prototype, ReadableStream.prototype);
ObjectSetPrototypeOf(InternalTransferredReadableStream, ReadableStream);

function TransferredReadableStream() {
  const stream = new InternalTransferredReadableStream();

  stream.constructor = ReadableStream;

  return stream;
}

TransferredReadableStream.prototype[kDeserialize] = () => {};

class ReadableStreamBYOBRequest {
  [kType] = 'ReadableStreamBYOBRequest';

  constructor(skipThrowSymbol = undefined) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }

  /**
   * @readonly
   * @type {Uint8Array}
   */
  get view() {
    if (!isReadableStreamBYOBRequest(this))
      throw new ERR_INVALID_THIS('ReadableStreamBYOBRequest');
    return this[kState].view;
  }

  /**
   * @param {number} bytesWritten
   */
  respond(bytesWritten) {
    if (!isReadableStreamBYOBRequest(this))
      throw new ERR_INVALID_THIS('ReadableStreamBYOBRequest');
    const {
      view,
      controller,
    } = this[kState];
    if (controller === undefined) {
      throw new ERR_INVALID_STATE.TypeError(
        'This BYOB request has been invalidated');
    }

    const viewByteLength = ArrayBufferViewGetByteLength(view);
    const viewBuffer = ArrayBufferViewGetBuffer(view);
    const viewBufferByteLength = ArrayBufferPrototypeGetByteLength(viewBuffer);

    if (ArrayBufferPrototypeGetDetached(viewBuffer)) {
      throw new ERR_INVALID_STATE.TypeError('Viewed ArrayBuffer is detached');
    }

    assert(viewByteLength > 0);
    assert(viewBufferByteLength > 0);

    readableByteStreamControllerRespond(controller, bytesWritten);
  }

  /**
   * @param {ArrayBufferView} view
   */
  respondWithNewView(view) {
    if (!isReadableStreamBYOBRequest(this))
      throw new ERR_INVALID_THIS('ReadableStreamBYOBRequest');
    const {
      controller,
    } = this[kState];

    if (controller === undefined) {
      throw new ERR_INVALID_STATE.TypeError(
        'This BYOB request has been invalidated');
    }

    validateBuffer(view, 'view');

    if (ArrayBufferPrototypeGetDetached(view.buffer)) {
      throw new ERR_INVALID_STATE.TypeError('Viewed ArrayBuffer is detached');
    }

    readableByteStreamControllerRespondWithNewView(controller, view);
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      view: this.view,
      controller: this[kState].controller,
    });
  }
}

ObjectDefineProperties(ReadableStreamBYOBRequest.prototype, {
  view: kEnumerableProperty,
  respond: kEnumerableProperty,
  respondWithNewView: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(ReadableStreamBYOBRequest.name),
});

function createReadableStreamBYOBRequest(controller, view) {
  const stream = new ReadableStreamBYOBRequest(kSkipThrow);

  stream[kState] = {
    controller,
    view,
  };

  return stream;
}

class ReadableStreamAsyncIteratorReadRequest {
  constructor(reader, state, promise) {
    this.reader = reader;
    this.state = state;
    this.promise = promise;
  }

  [kChunk](chunk) {
    this.state.current = undefined;
    this.promise.resolve({ done: false, value: chunk });
  }

  [kClose]() {
    this.state.current = undefined;
    this.state.done = true;
    readableStreamReaderGenericRelease(this.reader);
    this.promise.resolve({ done: true, value: undefined });
  }

  [kError](error) {
    this.state.current = undefined;
    this.state.done = true;
    readableStreamReaderGenericRelease(this.reader);
    this.promise.reject(error);
  }
}

class DefaultReadRequest {
  constructor() {
    this[kState] = PromiseWithResolvers();
  }

  [kChunk](value) {
    this[kState].resolve({ done: false, value });
  }

  [kClose]() {
    this[kState].resolve({ done: true, value: undefined });
  }

  [kError](error) {
    this[kState].reject(error);
  }

  get promise() { return this[kState].promise; }
}

class ReadIntoRequest {
  constructor() {
    this[kState] = PromiseWithResolvers();
  }

  [kChunk](value) {
    this[kState].resolve({ done: false, value });
  }

  [kClose](value) {
    this[kState].resolve({ done: true, value });
  }

  [kError](error) {
    this[kState].reject(error);
  }

  get promise() { return this[kState].promise; }
}

class ReadableStreamDefaultReader {
  [kType] = 'ReadableStreamDefaultReader';

  /**
   * @param {ReadableStream} stream
   */
  constructor(stream) {
    if (!isReadableStream(stream))
      throw new ERR_INVALID_ARG_TYPE('stream', 'ReadableStream', stream);
    this[kState] = {
      // All fields are unconditionally assigned during setup.
      readRequests: undefined,
      stream: undefined,
      close: undefined,
    };
    setupReadableStreamDefaultReader(this, stream);
  }

  /**
   * @returns {Promise<{
   *   value : any,
   *   done : boolean
   * }>}
   */
  read() {
    if (!isReadableStreamDefaultReader(this))
      return PromiseReject(new ERR_INVALID_THIS('ReadableStreamDefaultReader'));
    if (this[kState].stream === undefined) {
      return PromiseReject(
        new ERR_INVALID_STATE.TypeError(
          'The reader is not attached to a stream'));
    }

    const stream = this[kState].stream;
    const controller = stream[kState].controller;

    // Fast path: if data is already buffered in the controller's queue,
    // return a resolved promise immediately without creating a read request.
    // This is spec-compliant because read() returns a Promise, and
    // Promise.resolve() callbacks still run in the microtask queue.
    if (stream[kState].state === 'readable') {
      if (isReadableStreamDefaultController(controller)) {
        if (controller[kState].queue.length > 0) {
          stream[kState].disturbed = true;
          const chunk = dequeueValue(controller);

          if (controller[kState].closeRequested && !controller[kState].queue.length) {
            readableStreamDefaultControllerClearAlgorithms(controller);
            readableStreamClose(stream);
          } else if (!controller[kState].closeRequested &&
                     controller[kState].started &&
                     controller[kState].highWaterMark -
                       controller[kState].queueTotalSize > 0) {
            // ShouldCallPull reduced to the conditions not already
            // established on this path: the state is readable and the
            // queue was non-empty, so no read requests can be parked.
            readableStreamDefaultControllerPull(controller);
          }

          return PromiseResolve({ done: false, value: chunk });
        }
      } else if (controller[kState].queueTotalSize > 0) {
        // Byte controller with buffered data: mirror the queue-filled arm
        // of its pull steps (which never consults pendingPullIntos) minus
        // the read request.
        stream[kState].disturbed = true;
        return PromiseResolve({
          done: false,

          value: readableByteStreamControllerDequeueChunk(controller),
        });
      }
    }

    // Slow path: create request and go through normal flow
    const readRequest = new DefaultReadRequest();
    readableStreamDefaultReaderRead(this, readRequest);
    return readRequest.promise;
  }

  releaseLock() {
    if (!isReadableStreamDefaultReader(this))
      throw new ERR_INVALID_THIS('ReadableStreamDefaultReader');
    if (this[kState].stream === undefined)
      return;
    readableStreamDefaultReaderRelease(this);
  }

  /**
   * @readonly
   * @type {Promise<void>}
   */
  get closed() {
    if (!isReadableStreamDefaultReader(this))
      return PromiseReject(new ERR_INVALID_THIS('ReadableStreamDefaultReader'));
    return readerClosedPromise(this).promise;
  }

  /**
   * @param {any} [reason]
   * @returns {Promise<void>}
   */
  cancel(reason = undefined) {
    if (!isReadableStreamDefaultReader(this))
      return PromiseReject(new ERR_INVALID_THIS('ReadableStreamDefaultReader'));
    if (this[kState].stream === undefined) {
      return PromiseReject(new ERR_INVALID_STATE.TypeError(
        'The reader is not attached to a stream'));
    }
    return readableStreamReaderGenericCancel(this, reason);
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      stream: this[kState].stream,
      readRequests: this[kState].readRequests.length,
      close: readerClosedPromise(this).promise,
    });
  }
}

ObjectDefineProperties(ReadableStreamDefaultReader.prototype, {
  closed: kEnumerableProperty,
  read: kEnumerableProperty,
  releaseLock: kEnumerableProperty,
  cancel: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(ReadableStreamDefaultReader.name),
});

class ReadableStreamBYOBReader {
  [kType] = 'ReadableStreamBYOBReader';

  /**
   * @param {ReadableStream} stream
   */
  constructor(stream) {
    if (!isReadableStream(stream))
      throw new ERR_INVALID_ARG_TYPE('stream', 'ReadableStream', stream);
    this[kState] = {
      // All fields are unconditionally assigned during setup.
      stream: undefined,
      readIntoRequests: undefined,
      close: undefined,
    };
    setupReadableStreamBYOBReader(this, stream);
  }

  /**
   * @param {ArrayBufferView} view
   * @param {{
   *   min? : number
   * }} [options]
   * @returns {Promise<{
   *   value : ArrayBufferView,
   *   done : boolean,
   * }>}
   */
  async read(view, options = kEmptyObject) {
    if (!isReadableStreamBYOBReader(this))
      throw new ERR_INVALID_THIS('ReadableStreamBYOBReader');
    if (!isArrayBufferView(view)) {
      throw new ERR_INVALID_ARG_TYPE(
        'view',
        [
          'Buffer',
          'TypedArray',
          'DataView',
        ],
        view,
      );
    }
    validateObject(options, 'options', kValidateObjectAllowObjectsAndNull);

    const viewByteLength = ArrayBufferViewGetByteLength(view);
    const viewBuffer = ArrayBufferViewGetBuffer(view);

    if (isSharedArrayBuffer(viewBuffer)) {
      throw new ERR_INVALID_ARG_VALUE(
        'view',
        view,
        'must not be backed by a SharedArrayBuffer',
      );
    }

    const viewBufferByteLength = ArrayBufferPrototypeGetByteLength(viewBuffer);

    if (viewByteLength === 0 || viewBufferByteLength === 0) {
      throw new ERR_INVALID_STATE.TypeError(
        'View or Viewed ArrayBuffer is zero-length or detached');
    }

    // Supposed to assert here that the view's buffer is not
    // detached, but there's no API available to use to check that.

    const min = options?.min ?? 1;
    if (typeof min !== 'number')
      throw new ERR_INVALID_ARG_TYPE('options.min', 'number', min);
    if (!NumberIsInteger(min))
      throw new ERR_INVALID_ARG_VALUE('options.min', min, 'must be an integer');
    if (min <= 0)
      throw new ERR_INVALID_ARG_VALUE('options.min', min, 'must be greater than 0');
    if (!isDataView(view)) {
      if (min > TypedArrayPrototypeGetLength(view)) {
        throw new ERR_OUT_OF_RANGE('options.min', '<= view.length', min);
      }
    } else if (min > viewByteLength) {
      throw new ERR_OUT_OF_RANGE('options.min', '<= view.byteLength', min);
    }

    if (this[kState].stream === undefined) {
      throw new ERR_INVALID_STATE.TypeError('The reader is not attached to a stream');
    }
    const readIntoRequest = new ReadIntoRequest();
    readableStreamBYOBReaderRead(this, view, min, readIntoRequest);
    return readIntoRequest.promise;
  }

  releaseLock() {
    if (!isReadableStreamBYOBReader(this))
      throw new ERR_INVALID_THIS('ReadableStreamBYOBReader');
    if (this[kState].stream === undefined)
      return;
    readableStreamBYOBReaderRelease(this);
  }

  /**
   * @readonly
   * @type {Promise<void>}
   */
  get closed() {
    if (!isReadableStreamBYOBReader(this))
      return PromiseReject(new ERR_INVALID_THIS('ReadableStreamBYOBReader'));
    return readerClosedPromise(this).promise;
  }

  /**
   * @param {any} [reason]
   * @returns {Promise<void>}
   */
  cancel(reason = undefined) {
    if (!isReadableStreamBYOBReader(this))
      return PromiseReject(new ERR_INVALID_THIS('ReadableStreamBYOBReader'));
    if (this[kState].stream === undefined) {
      return PromiseReject(new ERR_INVALID_STATE.TypeError(
        'The reader is not attached to a stream'));
    }
    return readableStreamReaderGenericCancel(this, reason);
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      stream: this[kState].stream,
      readIntoRequests: this[kState].readIntoRequests.length,
      close: readerClosedPromise(this).promise,
    });
  }
}

ObjectDefineProperties(ReadableStreamBYOBReader.prototype, {
  closed: kEnumerableProperty,
  read: kEnumerableProperty,
  releaseLock: kEnumerableProperty,
  cancel: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(ReadableStreamBYOBReader.name),
});

class ReadableStreamDefaultController {
  [kType] = 'ReadableStreamDefaultController';
  [kState] = {};

  constructor(skipThrowSymbol = undefined) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }

  /**
   * @readonly
   * @type {number}
   */
  get desiredSize() {
    return readableStreamDefaultControllerGetDesiredSize(this);
  }

  close() {
    if (!readableStreamDefaultControllerCanCloseOrEnqueue(this))
      throw new ERR_INVALID_STATE.TypeError('Controller is already closed');
    readableStreamDefaultControllerClose(this);
  }

  /**
   * @param {any} [chunk]
   */
  enqueue(chunk = undefined) {
    if (!readableStreamDefaultControllerCanCloseOrEnqueue(this))
      throw new ERR_INVALID_STATE.TypeError('Controller is already closed');
    readableStreamDefaultControllerEnqueue(this, chunk);
  }

  /**
   * @param {any} [error]
   */
  error(error = undefined) {
    readableStreamDefaultControllerError(this, error);
  }

  [kCancel](reason) {
    return readableStreamDefaultControllerCancelSteps(this, reason);
  }

  [kPull](readRequest) {
    readableStreamDefaultControllerPullSteps(this, readRequest);
  }

  [kRelease]() {}

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], { });
  }
}

ObjectDefineProperties(ReadableStreamDefaultController.prototype, {
  desiredSize: kEnumerableProperty,
  close: kEnumerableProperty,
  enqueue: kEnumerableProperty,
  error: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(ReadableStreamDefaultController.name),
});

class ReadableByteStreamController {
  [kType] = 'ReadableByteStreamController';
  [kState] = {};

  constructor(skipThrowSymbol = undefined) {
    if (skipThrowSymbol !== kSkipThrow) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
  }

  /**
   * @readonly
   * @type {ReadableStreamBYOBRequest}
   */
  get byobRequest() {
    if (!isReadableByteStreamController(this))
      throw new ERR_INVALID_THIS('ReadableByteStreamController');
    if (this[kState].byobRequest === null &&
        this[kState].pendingPullIntos.length) {
      const {
        buffer,
        byteOffset,
        bytesFilled,
        byteLength,
      } = this[kState].pendingPullIntos[0];
      const view =
        new Uint8Array(
          buffer,
          byteOffset + bytesFilled,
          byteLength - bytesFilled);
      this[kState].byobRequest = createReadableStreamBYOBRequest(this, view);
    }
    return this[kState].byobRequest;
  }

  /**
   * @readonly
   * @type {number}
   */
  get desiredSize() {
    if (!isReadableByteStreamController(this))
      throw new ERR_INVALID_THIS('ReadableByteStreamController');
    return readableByteStreamControllerGetDesiredSize(this);
  }

  close() {
    if (!isReadableByteStreamController(this))
      throw new ERR_INVALID_THIS('ReadableByteStreamController');
    if (this[kState].closeRequested)
      throw new ERR_INVALID_STATE.TypeError('Controller is already closed');
    if (this[kState].stream[kState].state !== 'readable')
      throw new ERR_INVALID_STATE.TypeError('ReadableStream is already closed');
    readableByteStreamControllerClose(this);
  }

  /**
   * @param {ArrayBufferView} chunk
   */
  enqueue(chunk) {
    if (!isReadableByteStreamController(this))
      throw new ERR_INVALID_THIS('ReadableByteStreamController');
    validateBuffer(chunk);
    const chunkByteLength = ArrayBufferViewGetByteLength(chunk);
    const chunkBuffer = ArrayBufferViewGetBuffer(chunk);

    if (isSharedArrayBuffer(chunkBuffer)) {
      throw new ERR_INVALID_ARG_VALUE(
        'chunk',
        chunk,
        'must not be backed by a SharedArrayBuffer',
      );
    }

    const chunkBufferByteLength = ArrayBufferPrototypeGetByteLength(chunkBuffer);
    if (chunkByteLength === 0 || chunkBufferByteLength === 0) {
      throw new ERR_INVALID_STATE.TypeError(
        'chunk ArrayBuffer is zero-length or detached');
    }
    if (this[kState].closeRequested)
      throw new ERR_INVALID_STATE.TypeError('Controller is already closed');
    if (this[kState].stream[kState].state !== 'readable')
      throw new ERR_INVALID_STATE.TypeError('ReadableStream is already closed');
    readableByteStreamControllerEnqueue(this, chunk);
  }

  /**
   * @param {any} [error]
   */
  error(error = undefined) {
    if (!isReadableByteStreamController(this))
      throw new ERR_INVALID_THIS('ReadableByteStreamController');
    readableByteStreamControllerError(this, error);
  }

  [kCancel](reason) {
    return readableByteStreamControllerCancelSteps(this, reason);
  }

  [kPull](readRequest) {
    readableByteStreamControllerPullSteps(this, readRequest);
  }

  [kRelease]() {
    const {
      pendingPullIntos,
    } = this[kState];
    if (pendingPullIntos.length > 0) {
      const firstPendingPullInto = pendingPullIntos[0];
      firstPendingPullInto.type = 'none';
      this[kState].pendingPullIntos = [firstPendingPullInto];
    }
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], { });
  }
}

ObjectDefineProperties(ReadableByteStreamController.prototype, {
  byobRequest: kEnumerableProperty,
  desiredSize: kEnumerableProperty,
  close: kEnumerableProperty,
  enqueue: kEnumerableProperty,
  error: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(ReadableByteStreamController.name),
});

function InternalReadableStream(start, pull, cancel, highWaterMark, size) {
  ObjectSetPrototypeOf(this, ReadableStream.prototype);
  markTransferMode(this, false, true);
  this[kType] = 'ReadableStream';
  this[kState] = createReadableStreamState();
  const controller = new ReadableStreamDefaultController(kSkipThrow);
  setupReadableStreamDefaultController(
    this,
    controller,
    start,
    pull,
    cancel,
    highWaterMark,
    size);
}

ObjectSetPrototypeOf(InternalReadableStream.prototype, ReadableStream.prototype);
ObjectSetPrototypeOf(InternalReadableStream, ReadableStream);

function createReadableStream(start, pull, cancel, highWaterMark = 1, size = defaultSizeAlgorithm) {
  const stream = new InternalReadableStream(start, pull, cancel, highWaterMark, size);

  // For spec compliance the InternalReadableStream must be a ReadableStream
  stream.constructor = ReadableStream;
  return stream;
}

function InternalReadableByteStream(start, pull, cancel) {
  ObjectSetPrototypeOf(this, ReadableStream.prototype);
  markTransferMode(this, false, true);
  this[kType] = 'ReadableStream';
  this[kState] = createReadableStreamState();
  const controller = new ReadableByteStreamController(kSkipThrow);
  setupReadableByteStreamController(
    this,
    controller,
    start,
    pull,
    cancel,
    0,
    undefined);
}

ObjectSetPrototypeOf(InternalReadableByteStream.prototype, ReadableStream.prototype);
ObjectSetPrototypeOf(InternalReadableByteStream, ReadableStream);

function createReadableByteStream(start, pull, cancel) {
  const stream = new InternalReadableByteStream(start, pull, cancel);

  // For spec compliance the InternalReadableByteStream must be a ReadableStream
  stream.constructor = ReadableStream;
  return stream;
}

const isReadableStream =
  isBrandCheck('ReadableStream');
const isReadableByteStreamController =
  isBrandCheck('ReadableByteStreamController');
const isReadableStreamDefaultController =
  isBrandCheck('ReadableStreamDefaultController');
const isReadableStreamBYOBRequest =
  isBrandCheck('ReadableStreamBYOBRequest');
const isReadableStreamDefaultReader =
  isBrandCheck('ReadableStreamDefaultReader');
const isReadableStreamBYOBReader =
  isBrandCheck('ReadableStreamBYOBReader');

// ---- ReadableStream Implementation

function createReadableStreamState() {
  return {
    __proto__: null,
    closedPromise: undefined,
    disturbed: false,
    reader: undefined,
    state: 'readable',
    storedError: undefined,
    transfer: {
      __proto__: null,
      writable: undefined,
      port1: undefined,
      port2: undefined,
      promise: undefined,
    },
  };
}

function readableStreamFromIterable(iterable) {
  let stream;
  const iteratorGetter = iterable[SymbolAsyncIterator] ?? iterable[SymbolIterator];
  if (iteratorGetter == null || typeof iteratorGetter !== 'function') {
    throw new ERR_ARG_NOT_ITERABLE(iterable);
  }
  const iterator = FunctionPrototypeCall(iteratorGetter, iterable);
  if (iterator === null || (typeof iterator !== 'object' && typeof iterator !== 'function')) {
    throw new ERR_INVALID_STATE.TypeError('The iterator method must return an object');
  }
  const startAlgorithm = nonOpStart;

  async function pullAlgorithm() {
    const iterResult = await iterator.next();
    if (typeof iterResult !== 'object' || iterResult === null) {
      throw new ERR_INVALID_STATE.TypeError(
        'The promise returned by the iterator.next() method must fulfill with an object');
    }
    if (iterResult.done) {
      readableStreamDefaultControllerClose(stream[kState].controller);
    } else {
      readableStreamDefaultControllerEnqueue(stream[kState].controller, await iterResult.value);
    }
  }

  async function cancelAlgorithm(reason) {
    const returnMethod = iterator.return;
    if (returnMethod === undefined) {
      return;
    }
    const iterResult = await FunctionPrototypeCall(returnMethod, iterator, reason);
    if (typeof iterResult !== 'object' || iterResult === null) {
      throw new ERR_INVALID_STATE.TypeError(
        'The promise returned by the iterator.return() method must fulfill with an object');
    }
  }

  stream = createReadableStream(
    startAlgorithm,
    pullAlgorithm,
    cancelAlgorithm,
    0,
  );

  return stream;
}

function readableStreamPipeTo(
  source,
  dest,
  preventClose,
  preventAbort,
  preventCancel,
  signal) {

  let reader;
  let writer;
  let disposable;
  // Both of these can throw synchronously. We want to capture
  // the error and return a rejected promise instead.
  try {
    reader = new ReadableStreamDefaultReader(source);
    writer = new WritableStreamDefaultWriter(dest);
  } catch (error) {
    return PromiseReject(error);
  }

  source[kState].disturbed = true;

  let shuttingDown = false;

  if (signal !== undefined) {
    try {
      validateAbortSignal(signal, 'options.signal');
    } catch (error) {
      return PromiseReject(error);
    }
  }

  const promise = PromiseWithResolvers();

  const state = {
    currentWrite: PromiseResolve(),
  };

  // The error here can be undefined. The rejected arg
  // tells us that the promise must be rejected even
  // when error is undefine.
  function finalize(rejected, error) {
    writableStreamDefaultWriterRelease(writer);
    readableStreamReaderGenericRelease(reader);
    if (signal !== undefined)
      disposable?.[SymbolDispose]();
    if (rejected)
      promise.reject(error);
    else
      promise.resolve();
  }

  async function waitForCurrentWrite() {
    const write = state.currentWrite;
    await write;
    if (write !== state.currentWrite)
      await waitForCurrentWrite();
  }

  function shutdownWithAnAction(action, rejected, originalError) {
    if (shuttingDown) return;
    shuttingDown = true;
    if (dest[kState].state === 'writable' &&
        !writableStreamCloseQueuedOrInFlight(dest)) {
      PromisePrototypeThen(
        waitForCurrentWrite(),
        complete,
        (error) => finalize(true, error));
      return;
    }
    complete();

    function complete() {
      PromisePrototypeThen(
        action(),
        () => finalize(rejected, originalError),
        (error) => finalize(true, error));
    }
  }

  function shutdown(rejected, error) {
    if (shuttingDown) return;
    shuttingDown = true;
    if (dest[kState].state === 'writable' &&
        !writableStreamCloseQueuedOrInFlight(dest)) {
      PromisePrototypeThen(
        waitForCurrentWrite(),
        () => finalize(rejected, error),
        (error) => finalize(true, error));
      return;
    }
    finalize(rejected, error);
  }

  function abortAlgorithm() {
    let error;
    if (signal.reason instanceof AbortError) {
      // Cannot use the AbortError class here. It must be a DOMException.
      error = new DOMException(signal.reason.message, 'AbortError');
    } else {
      error = signal.reason;
    }

    const actions = [];
    if (!preventAbort) {
      ArrayPrototypePush(
        actions,
        () => {
          if (dest[kState].state === 'writable')
            return writableStreamAbort(dest, error);
          return PromiseResolve();
        });
    }
    if (!preventCancel) {
      ArrayPrototypePush(
        actions,
        () => {
          if (source[kState].state === 'readable')
            return readableStreamCancel(source, error);
          return PromiseResolve();
        });
    }

    shutdownWithAnAction(
      () => SafePromiseAll(actions, (action) => action()),
      true,
      error);
  }

  function watchErrored(stream, promise, action) {
    if (stream[kState].state === 'errored')
      action(stream[kState].storedError);
    else
      PromisePrototypeThen(promise, undefined, action);
  }

  function watchClosed(stream, promise, action) {
    if (stream[kState].state === 'closed')
      action();
    else
      PromisePrototypeThen(promise, action, () => {});
  }

  async function step() {
    if (shuttingDown) return true;

    if (dest[kState].backpressure) {
      await writerReadyPromise(writer).promise;
      if (shuttingDown) return true;
    }

    const controller = source[kState].controller;

    // Fast path: batch reads when data is buffered in a default controller.
    // This avoids creating PipeToReadableStreamReadRequest objects and
    // reduces promise allocation overhead.
    if (source[kState].state === 'readable' &&
        isReadableStreamDefaultController(controller) &&
        controller[kState].queue.length > 0) {

      while (controller[kState].queue.length > 0) {
        if (shuttingDown) return true;

        const chunk = dequeueValue(controller);

        if (controller[kState].closeRequested && !controller[kState].queue.length) {
          readableStreamDefaultControllerClearAlgorithms(controller);
          readableStreamClose(source);
        }

        // Write the chunk - we're already in a separate microtask from enqueue
        // because we awaited the writer ready promise above.
        state.currentWrite = writableStreamDefaultWriterWrite(writer, chunk);
        markPromiseAsHandled(state.currentWrite);

        // Check backpressure after each write
        if (dest[kState].backpressure) {
          // Backpressure - stop batch and wait for ready
          break;
        } else if (dest[kState].state !== 'writable' || writableStreamCloseQueuedOrInFlight(dest)) {
          // Closing or erroring - stop batch and wait for shutdown
          break;
        }
      }

      // Trigger pull if needed after batch
      readableStreamDefaultControllerCallPullIfNeeded(controller);

      // Check if stream closed during batch
      if (source[kState].state === 'closed') {
        return true;
      }

      // Yield to microtask queue between batches to allow events/signals to fire
      return false;
    }

    // Slow path: use read request for async reads
    const promise = PromiseWithResolvers();
    // eslint-disable-next-line no-use-before-define
    readableStreamDefaultReaderRead(reader, new PipeToReadableStreamReadRequest(writer, state, promise));

    return promise.promise;
  }

  async function run() {
    // Run until step resolves as true
    while (!await step());
  }

  if (signal !== undefined) {
    if (signal.aborted) {
      abortAlgorithm();
      return promise.promise;
    }
    addAbortListener ??= require('internal/events/abort_listener').addAbortListener;
    disposable = addAbortListener(signal, abortAlgorithm);
  }

  setPromiseHandled(run());

  watchErrored(source, readerClosedPromise(reader).promise, (error) => {
    if (!preventAbort) {
      return shutdownWithAnAction(
        () => writableStreamAbort(dest, error),
        true,
        error);
    }
    shutdown(true, error);
  });

  watchErrored(dest, writerClosedPromise(writer).promise, (error) => {
    if (!preventCancel) {
      return shutdownWithAnAction(
        () => readableStreamCancel(source, error),
        true,
        error);
    }
    shutdown(true, error);
  });

  watchClosed(source, readerClosedPromise(reader).promise, () => {
    if (!preventClose) {
      return shutdownWithAnAction(
        () => writableStreamDefaultWriterCloseWithErrorPropagation(writer));
    }
    shutdown();
  });

  if (writableStreamCloseQueuedOrInFlight(dest) ||
      dest[kState].state === 'closed') {
    const error = new ERR_INVALID_STATE.TypeError(
      'Destination WritableStream is closed');
    if (!preventCancel) {
      shutdownWithAnAction(
        () => readableStreamCancel(source, error), true, error);
    } else {
      shutdown(true, error);
    }
  }

  return promise.promise;
}

class PipeToReadableStreamReadRequest {
  constructor(writer, state, promise) {
    this.writer = writer;
    this.state = state;
    this.promise = promise;
  }

  [kChunk](chunk) {
    // Per spec, pipeTo must queue a microtask for the write to avoid
    // synchronous write during enqueue(). See WHATWG Streams spec
    // "ReadableStreamPipeTo" step 15's "chunk steps".
    queueMicrotask(() => {
      this.state.currentWrite = writableStreamDefaultWriterWrite(this.writer, chunk);
      markPromiseAsHandled(this.state.currentWrite);
      this.promise.resolve(false);
    });
  }

  [kClose]() {
    this.promise.resolve(true);
  }

  [kError](error) {
    this.promise.reject(error);
  }
}

function readableStreamTee(stream, cloneForBranch2) {
  if (isReadableByteStreamController(stream[kState].controller)) {
    return readableByteStreamTee(stream);
  }
  return readableStreamDefaultTee(stream, cloneForBranch2);
}

function readableStreamDefaultTee(stream, cloneForBranch2) {
  const reader = new ReadableStreamDefaultReader(stream);
  let reading = false;
  let canceled1 = false;
  let canceled2 = false;
  let reason1;
  let reason2;
  let branch1;
  let branch2;
  const cancelPromise = PromiseWithResolvers();

  async function pullAlgorithm() {
    if (reading) return;
    reading = true;
    const readRequest = {
      [kChunk](value) {
        queueMicrotask(() => {
          reading = false;
          const value1 = value;
          let value2 = value;
          if (!canceled2 && cloneForBranch2) {
            value2 = structuredClone(value2);
          }
          if (!canceled1) {
            readableStreamDefaultControllerEnqueue(
              branch1[kState].controller,
              value1);
          }
          if (!canceled2) {
            readableStreamDefaultControllerEnqueue(
              branch2[kState].controller,
              value2);
          }
        });
      },
      [kClose]() {
        // The `process.nextTick()` is not part of the spec.
        // This approach was needed to avoid a race condition working with esm
        // Further information, see: https://github.com/nodejs/node/issues/39758
        process.nextTick(() => {
          reading = false;
          if (!canceled1)
            readableStreamDefaultControllerClose(branch1[kState].controller);
          if (!canceled2)
            readableStreamDefaultControllerClose(branch2[kState].controller);
          if (!canceled1 || !canceled2)
            cancelPromise.resolve();
        });
      },
      [kError]() {
        reading = false;
      },
    };
    readableStreamDefaultReaderRead(reader, readRequest);
  }

  function cancel1Algorithm(reason) {
    canceled1 = true;
    reason1 = reason;
    if (canceled2) {
      const compositeReason = [reason1, reason2];
      cancelPromise.resolve(readableStreamCancel(stream, compositeReason));
    }
    return cancelPromise.promise;
  }

  function cancel2Algorithm(reason) {
    canceled2 = true;
    reason2 = reason;
    if (canceled1) {
      const compositeReason = [reason1, reason2];
      cancelPromise.resolve(readableStreamCancel(stream, compositeReason));
    }
    return cancelPromise.promise;
  }

  branch1 =
    createReadableStream(nonOpStart, pullAlgorithm, cancel1Algorithm);
  branch2 =
    createReadableStream(nonOpStart, pullAlgorithm, cancel2Algorithm);

  PromisePrototypeThen(
    readerClosedPromise(reader).promise,
    undefined,
    (error) => {
      readableStreamDefaultControllerError(branch1[kState].controller, error);
      readableStreamDefaultControllerError(branch2[kState].controller, error);
      if (!canceled1 || !canceled2)
        cancelPromise.resolve();
    });

  return [branch1, branch2];
}

function readableByteStreamTee(stream) {
  assert(isReadableStream(stream));
  assert(isReadableByteStreamController(stream[kState].controller));

  let reader = new ReadableStreamDefaultReader(stream);
  let reading = false;
  let readAgainForBranch1 = false;
  let readAgainForBranch2 = false;
  let canceled1 = false;
  let canceled2 = false;
  let reason1;
  let reason2;
  let branch1;
  let branch2;
  const cancelDeferred = PromiseWithResolvers();

  function forwardReaderError(thisReader) {
    PromisePrototypeThen(
      readerClosedPromise(thisReader).promise,
      undefined,
      (error) => {
        if (thisReader !== reader) {
          return;
        }
        readableStreamDefaultControllerError(branch1[kState].controller, error);
        readableStreamDefaultControllerError(branch2[kState].controller, error);
        if (!canceled1 || !canceled2) {
          cancelDeferred.resolve();
        }
      },
    );
  }

  function pullWithDefaultReader() {
    if (isReadableStreamBYOBReader(reader)) {
      readableStreamBYOBReaderRelease(reader);
      reader = new ReadableStreamDefaultReader(stream);
      forwardReaderError(reader);
    }

    const readRequest = {
      [kChunk](chunk) {
        queueMicrotask(() => {
          readAgainForBranch1 = false;
          readAgainForBranch2 = false;
          const chunk1 = chunk;
          let chunk2 = chunk;

          if (!canceled1 && !canceled2) {
            try {
              chunk2 = cloneAsUint8Array(chunk);
            } catch (error) {
              readableByteStreamControllerError(
                branch1[kState].controller,
                error,
              );
              readableByteStreamControllerError(
                branch2[kState].controller,
                error,
              );
              cancelDeferred.resolve(readableStreamCancel(stream, error));
              return;
            }
          }
          if (!canceled1) {
            readableByteStreamControllerEnqueue(
              branch1[kState].controller,
              chunk1,
            );
          }
          if (!canceled2) {
            readableByteStreamControllerEnqueue(
              branch2[kState].controller,
              chunk2,
            );
          }
          reading = false;

          if (readAgainForBranch1) {
            pull1Algorithm();
          } else if (readAgainForBranch2) {
            pull2Algorithm();
          }
        });
      },
      [kClose]() {
        reading = false;

        if (!canceled1) {
          readableByteStreamControllerClose(branch1[kState].controller);
        }
        if (!canceled2) {
          readableByteStreamControllerClose(branch2[kState].controller);
        }
        if (branch1[kState].controller[kState].pendingPullIntos.length > 0) {
          readableByteStreamControllerRespond(branch1[kState].controller, 0);
        }
        if (branch2[kState].controller[kState].pendingPullIntos.length > 0) {
          readableByteStreamControllerRespond(branch2[kState].controller, 0);
        }
        if (!canceled1 || !canceled2) {
          cancelDeferred.resolve();
        }
      },
      [kError]() {
        reading = false;
      },
    };

    readableStreamDefaultReaderRead(reader, readRequest);
  }

  function pullWithBYOBReader(view, forBranch2) {
    if (isReadableStreamDefaultReader(reader)) {
      readableStreamDefaultReaderRelease(reader);
      reader = new ReadableStreamBYOBReader(stream);
      forwardReaderError(reader);
    }

    const byobBranch = forBranch2 === true ? branch2 : branch1;
    const otherBranch = forBranch2 === false ? branch2 : branch1;
    const readIntoRequest = {
      [kChunk](chunk) {
        queueMicrotask(() => {
          readAgainForBranch1 = false;
          readAgainForBranch2 = false;
          const byobCanceled = forBranch2 === true ? canceled2 : canceled1;
          const otherCanceled = forBranch2 === false ? canceled2 : canceled1;

          if (!otherCanceled) {
            let clonedChunk;

            try {
              clonedChunk = cloneAsUint8Array(chunk);
            } catch (error) {
              readableByteStreamControllerError(
                byobBranch[kState].controller,
                error,
              );
              readableByteStreamControllerError(
                otherBranch[kState].controller,
                error,
              );
              cancelDeferred.resolve(readableStreamCancel(stream, error));
              return;
            }
            if (!byobCanceled) {
              readableByteStreamControllerRespondWithNewView(
                byobBranch[kState].controller,
                chunk,
              );
            }

            readableByteStreamControllerEnqueue(
              otherBranch[kState].controller,
              clonedChunk,
            );
          } else if (!byobCanceled) {
            readableByteStreamControllerRespondWithNewView(
              byobBranch[kState].controller,
              chunk,
            );
          }
          reading = false;

          if (readAgainForBranch1) {
            pull1Algorithm();
          } else if (readAgainForBranch2) {
            pull2Algorithm();
          }
        });
      },
      [kClose](chunk) {
        reading = false;

        const byobCanceled = forBranch2 === true ? canceled2 : canceled1;
        const otherCanceled = forBranch2 === false ? canceled2 : canceled1;

        if (!byobCanceled) {
          readableByteStreamControllerClose(byobBranch[kState].controller);
        }
        if (!otherCanceled) {
          readableByteStreamControllerClose(otherBranch[kState].controller);
        }
        if (chunk !== undefined) {
          if (!byobCanceled) {
            readableByteStreamControllerRespondWithNewView(
              byobBranch[kState].controller,
              chunk,
            );
          }
          if (
            !otherCanceled &&
            otherBranch[kState].controller[kState].pendingPullIntos.length > 0
          ) {
            readableByteStreamControllerRespond(
              otherBranch[kState].controller,
              0,
            );
          }
        }
        if (!byobCanceled || !otherCanceled) {
          cancelDeferred.resolve();
        }
      },
      [kError]() {
        reading = false;
      },
    };
    readableStreamBYOBReaderRead(reader, view, 1, readIntoRequest);
  }

  function pull1Algorithm() {
    if (reading) {
      readAgainForBranch1 = true;
      return PromiseResolve();
    }
    reading = true;

    const byobRequest = branch1[kState].controller.byobRequest;
    if (byobRequest === null) {
      pullWithDefaultReader();
    } else {
      pullWithBYOBReader(byobRequest[kState].view, false);
    }
    return PromiseResolve();
  }

  function pull2Algorithm() {
    if (reading) {
      readAgainForBranch2 = true;
      return PromiseResolve();
    }
    reading = true;

    const byobRequest = branch2[kState].controller.byobRequest;
    if (byobRequest === null) {
      pullWithDefaultReader();
    } else {
      pullWithBYOBReader(byobRequest[kState].view, true);
    }
    return PromiseResolve();
  }

  function cancel1Algorithm(reason) {
    canceled1 = true;
    reason1 = reason;
    if (canceled2) {
      cancelDeferred.resolve(readableStreamCancel(stream, [reason1, reason2]));
    }
    return cancelDeferred.promise;
  }

  function cancel2Algorithm(reason) {
    canceled2 = true;
    reason2 = reason;
    if (canceled1) {
      cancelDeferred.resolve(readableStreamCancel(stream, [reason1, reason2]));
    }
    return cancelDeferred.promise;
  }

  branch1 =
    createReadableByteStream(nonOpStart, pull1Algorithm, cancel1Algorithm);
  branch2 =
    createReadableByteStream(nonOpStart, pull2Algorithm, cancel2Algorithm);

  forwardReaderError(reader);

  return [branch1, branch2];
}

function readableByteStreamControllerConvertPullIntoDescriptor(desc) {
  const {
    buffer,
    bytesFilled,
    byteLength,
    byteOffset,
    ctor,
    elementSize,
  } = desc;
  if (bytesFilled > byteLength)
    throw new ERR_INVALID_STATE.RangeError('The buffer size is invalid');
  assert(!(bytesFilled % elementSize));
  const transferredBuffer = ArrayBufferPrototypeTransfer(buffer);

  if (ctor === Buffer) {
    return Buffer.from(transferredBuffer, byteOffset, bytesFilled / elementSize);
  }

  return new ctor(transferredBuffer, byteOffset, bytesFilled / elementSize);
}

function isReadableStreamLocked(stream) {
  return stream[kState].reader !== undefined;
}

function readableStreamCancel(stream, reason) {
  stream[kState].disturbed = true;
  switch (stream[kState].state) {
    case 'closed':
      return PromiseResolve();
    case 'errored':
      return PromiseReject(stream[kState].storedError);
  }
  readableStreamClose(stream);
  const {
    reader,
  } = stream[kState];
  if (reader !== undefined && readableStreamHasBYOBReader(stream)) {
    for (let n = 0; n < reader[kState].readIntoRequests.length; n++)
      reader[kState].readIntoRequests[n][kClose]();
    reader[kState].readIntoRequests = [];
  }

  return PromisePrototypeThen(
    stream[kState].controller[kCancel](reason),
    () => {});
}

function readableStreamClose(stream) {
  assert(stream[kState].state === 'readable');
  stream[kState].state = 'closed';
  stream[kState].closedPromise?.resolve();
  const {
    reader,
  } = stream[kState];

  if (reader === undefined)
    return;

  reader[kState].close?.resolve();

  if (readableStreamHasDefaultReader(stream)) {
    for (let n = 0; n < reader[kState].readRequests.length; n++)
      reader[kState].readRequests[n][kClose]();
    reader[kState].readRequests = [];
  }
}

function readableStreamError(stream, error) {
  assert(stream[kState].state === 'readable');
  stream[kState].state = 'errored';
  stream[kState].storedError = error;
  const closedPromiseCache = stream[kState].closedPromise;
  if (closedPromiseCache !== undefined) {
    setPromiseHandled(closedPromiseCache.promise);
    closedPromiseCache.reject(error);
  }

  const {
    reader,
  } = stream[kState];

  if (reader === undefined)
    return;

  const closeCache = reader[kState].close;
  if (closeCache !== undefined) {
    setPromiseHandled(closeCache.promise);
    closeCache.reject(error);
  }

  if (readableStreamHasDefaultReader(stream)) {
    for (let n = 0; n < reader[kState].readRequests.length; n++)
      reader[kState].readRequests[n][kError](error);
    reader[kState].readRequests = [];
  } else {
    assert(readableStreamHasBYOBReader(stream));
    for (let n = 0; n < reader[kState].readIntoRequests.length; n++)
      reader[kState].readIntoRequests[n][kError](error);
    reader[kState].readIntoRequests = [];
  }
}

function readableStreamHasDefaultReader(stream) {
  const {
    reader,
  } = stream[kState];

  if (reader === undefined)
    return false;

  return reader[kState] !== undefined &&
         reader[kType] === 'ReadableStreamDefaultReader';
}

function readableStreamGetNumReadRequests(stream) {
  assert(readableStreamHasDefaultReader(stream));
  return stream[kState].reader[kState].readRequests.length;
}

function readableStreamHasBYOBReader(stream) {
  const {
    reader,
  } = stream[kState];

  if (reader === undefined)
    return false;

  return reader[kState] !== undefined &&
         reader[kType] === 'ReadableStreamBYOBReader';
}

function readableStreamGetNumReadIntoRequests(stream) {
  assert(readableStreamHasBYOBReader(stream));
  return stream[kState].reader[kState].readIntoRequests.length;
}

function readableStreamFulfillReadRequest(stream, chunk, done) {
  assert(readableStreamHasDefaultReader(stream));
  const {
    reader,
  } = stream[kState];
  assert(reader[kState].readRequests.length);
  const readRequest = ArrayPrototypeShift(reader[kState].readRequests);

  // TODO(@jasnell): It's not clear under what exact conditions done
  // will be true here. The spec requires this check but none of the
  // WPT's or other tests trigger it. Will need to investigate how to
  // get coverage for this.
  if (done)
    readRequest[kClose]();
  else
    readRequest[kChunk](chunk);
}

function readableStreamFulfillReadIntoRequest(stream, chunk, done) {
  assert(readableStreamHasBYOBReader(stream));
  const {
    reader,
  } = stream[kState];
  assert(reader[kState].readIntoRequests.length);
  const readIntoRequest = ArrayPrototypeShift(reader[kState].readIntoRequests);
  if (done)
    readIntoRequest[kClose](chunk);
  else
    readIntoRequest[kChunk](chunk);
}

function readableStreamAddReadRequest(stream, readRequest) {
  assert(readableStreamHasDefaultReader(stream));
  assert(stream[kState].state === 'readable');
  ArrayPrototypePush(stream[kState].reader[kState].readRequests, readRequest);
}

function readableStreamAddReadIntoRequest(stream, readIntoRequest) {
  assert(readableStreamHasBYOBReader(stream));
  assert(stream[kState].state !== 'errored');
  ArrayPrototypePush(
    stream[kState].reader[kState].readIntoRequests,
    readIntoRequest);
}

function readableStreamReaderGenericCancel(reader, reason) {
  const {
    stream,
  } = reader[kState];
  assert(stream !== undefined);
  return readableStreamCancel(stream, reason);
}

function readableStreamReaderGenericInitialize(reader, stream) {
  reader[kState].stream = stream;
  stream[kState].reader = reader;
  // The reader's closed promise (reader[kState].close) is materialized
  // lazily by readerClosedPromise(); its settlement is fully derivable
  // from the reader/stream state, so nothing is allocated here.
}

// Materializes the reader's [[closedPromise]] record on first
// observation. While unobserved, its settlement is derivable: a
// detached reader was released (rejected with the shared released
// error); otherwise it follows the stream state. The settle sites
// (readableStreamClose/Error, readableStreamReaderGenericRelease)
// only touch the record when it has been materialized.
function readerClosedPromise(reader) {
  let close = reader[kState].close;
  if (close === undefined) {
    const stream = reader[kState].stream;
    if (stream === undefined) {
      close = {
        promise: PromiseReject(lazyReadableReleasedError()),
        resolve: undefined,
        reject: undefined,
      };
      setPromiseHandled(close.promise);
    } else {
      switch (stream[kState].state) {
        case 'readable':
          close = PromiseWithResolvers();
          break;
        case 'closed':
          close = {
            promise: PromiseResolve(),
            resolve: undefined,
            reject: undefined,
          };
          break;
        case 'errored':
          close = {
            promise: PromiseReject(stream[kState].storedError),
            resolve: undefined,
            reject: undefined,
          };
          setPromiseHandled(close.promise);
          break;
      }
    }
    reader[kState].close = close;
  }
  return close;
}

function readableStreamDefaultReaderRelease(reader) {
  readableStreamReaderGenericRelease(reader);
  readableStreamDefaultReaderErrorReadRequests(
    reader,
    lazyReadableReleasingError(),
  );
}

function readableStreamDefaultReaderErrorReadRequests(reader, e) {
  for (let n = 0; n < reader[kState].readRequests.length; ++n) {
    reader[kState].readRequests[n][kError](e);
  }
  reader[kState].readRequests = [];
}

function readableStreamBYOBReaderRelease(reader) {
  readableStreamReaderGenericRelease(reader);
  readableStreamBYOBReaderErrorReadIntoRequests(
    reader,
    lazyReadableReleasingError(),
  );
}

function readableStreamBYOBReaderErrorReadIntoRequests(reader, e) {
  for (let n = 0; n < reader[kState].readIntoRequests.length; ++n) {
    reader[kState].readIntoRequests[n][kError](e);
  }
  reader[kState].readIntoRequests = [];
}

function readableStreamReaderGenericRelease(reader) {
  const {
    stream,
  } = reader[kState];
  assert(stream !== undefined);
  assert(stream[kState].reader === reader);

  const closeCache = reader[kState].close;
  if (stream[kState].state === 'readable') {
    if (closeCache !== undefined) {
      closeCache.reject(lazyReadableReleasedError());
      setPromiseHandled(closeCache.promise);
    }
  } else {
    // The spec replaces [[closedPromise]] here. Dropping the cache makes
    // the next observation derive the released rejection.
    reader[kState].close = undefined;
  }

  stream[kState].controller[kRelease]();

  stream[kState].reader = undefined;
  reader[kState].stream = undefined;
}

function readableStreamBYOBReaderRead(reader, view, min, readIntoRequest) {
  const {
    stream,
  } = reader[kState];
  assert(stream !== undefined);
  stream[kState].disturbed = true;
  if (stream[kState].state === 'errored') {
    readIntoRequest[kError](stream[kState].storedError);
    return;
  }
  readableByteStreamControllerPullInto(
    stream[kState].controller,
    view,
    min,
    readIntoRequest);
}

function readableStreamDefaultReaderRead(reader, readRequest) {
  const {
    stream,
  } = reader[kState];
  assert(stream !== undefined);
  stream[kState].disturbed = true;
  switch (stream[kState].state) {
    case 'closed':
      readRequest[kClose]();
      break;
    case 'errored':
      readRequest[kError](stream[kState].storedError);
      break;
    case 'readable':
      stream[kState].controller[kPull](readRequest);
  }
}

function setupReadableStreamBYOBReader(reader, stream) {
  if (isReadableStreamLocked(stream))
    throw new ERR_INVALID_STATE.TypeError('ReadableStream is locked');
  const {
    controller,
  } = stream[kState];
  if (!isReadableByteStreamController(controller))
    throw new ERR_INVALID_ARG_VALUE('stream', stream, 'must be a byte stream');
  readableStreamReaderGenericInitialize(reader, stream);
  reader[kState].readIntoRequests = [];
}

function setupReadableStreamDefaultReader(reader, stream) {
  if (isReadableStreamLocked(stream))
    throw new ERR_INVALID_STATE.TypeError('ReadableStream is locked');
  readableStreamReaderGenericInitialize(reader, stream);
  reader[kState].readRequests = [];
}

function readableStreamDefaultControllerClose(controller) {
  if (!readableStreamDefaultControllerCanCloseOrEnqueue(controller))
    return;
  controller[kState].closeRequested = true;
  if (!controller[kState].queue.length) {
    readableStreamDefaultControllerClearAlgorithms(controller);
    readableStreamClose(controller[kState].stream);
  }
}

function readableStreamDefaultControllerEnqueue(controller, chunk) {
  // Equivalent to readableStreamDefaultControllerCanCloseOrEnqueue()
  // followed by isReadableStreamLocked() and
  // readableStreamGetNumReadRequests(), but with the state loaded once:
  // this runs for every enqueued chunk.
  const controllerState = controller[kState];
  const stream = controllerState.stream;
  if (controllerState.closeRequested || stream[kState].state !== 'readable')
    return;

  const reader = stream[kState].reader;
  if (reader !== undefined &&
      reader[kState] !== undefined &&
      reader[kType] === 'ReadableStreamDefaultReader' &&
      reader[kState].readRequests.length) {
    // Fulfilling a read request can run user code synchronously (the
    // pipeTo read request invokes the sink's write algorithm), so the
    // full ShouldCallPull predicate has to be re-evaluated afterwards.
    readableStreamFulfillReadRequest(stream, chunk, false);
  } else if (controllerState.sizeAlgorithm === defaultSizeAlgorithm) {
    // The internal default size algorithm is never observable by user
    // code, always returns 1, and cannot throw: enqueue with the
    // constant size instead of calling it or validating the result.
    // No user code runs between the guards at the top of this function
    // and this point, so ShouldCallPull reduces to the started flag and
    // the desired size (this branch implies no parked read requests,
    // ruling out the reader arm of the predicate).
    materializeQueue(controllerState).pushPair(chunk, 1);
    controllerState.queueTotalSize++;
    if (controllerState.started &&
        controllerState.highWaterMark - controllerState.queueTotalSize > 0) {
      readableStreamDefaultControllerPull(controller);
    }
    return;
  } else {
    // The user-supplied size algorithm may run arbitrary code, so the
    // full ShouldCallPull predicate has to be re-evaluated afterwards.
    try {
      const chunkSize =
        FunctionPrototypeCall(
          controllerState.sizeAlgorithm,
          undefined,
          chunk);
      enqueueValueWithSize(controller, chunk, chunkSize);
    } catch (error) {
      readableStreamDefaultControllerError(controller, error);
      throw error;
    }
  }
  readableStreamDefaultControllerCallPullIfNeeded(controller);
}

function readableStreamDefaultControllerHasBackpressure(controller) {
  return !readableStreamDefaultControllerShouldCallPull(controller);
}

function readableStreamDefaultControllerCanCloseOrEnqueue(controller) {
  const {
    stream,
  } = controller[kState];
  return !controller[kState].closeRequested &&
         stream[kState].state === 'readable';
}

function readableStreamDefaultControllerGetDesiredSize(controller) {
  const {
    stream,
    highWaterMark,
    queueTotalSize,
  } = controller[kState];
  switch (stream[kState].state) {
    case 'errored': return null;
    case 'closed': return 0;
    default:
      return highWaterMark - queueTotalSize;
  }
}

function readableStreamDefaultControllerShouldCallPull(controller) {
  // Single-pass version of the spec's predicate chain (CanCloseOrEnqueue,
  // IsLocked, HasDefaultReader, GetNumReadRequests, GetDesiredSize): this
  // runs at least once per chunk on every default-stream path. The
  // desired-size computation is inlined because the stream state is
  // already known to be 'readable' here.
  const controllerState = controller[kState];
  const stream = controllerState.stream;
  if (controllerState.closeRequested ||
      stream[kState].state !== 'readable' ||
      !controllerState.started)
    return false;

  const reader = stream[kState].reader;
  if (reader !== undefined &&
      reader[kState] !== undefined &&
      reader[kType] === 'ReadableStreamDefaultReader' &&
      reader[kState].readRequests.length) {
    return true;
  }

  return controllerState.highWaterMark - controllerState.queueTotalSize > 0;
}

function readableStreamDefaultControllerCallPullIfNeeded(controller) {
  if (!readableStreamDefaultControllerShouldCallPull(controller))
    return;
  readableStreamDefaultControllerPull(controller);
}

// The ShouldCallPull half of CallPullIfNeeded, split out so that callers
// that have already established the predicate from state in scope (the
// enqueue path above) can skip re-running it.
function readableStreamDefaultControllerPull(controller) {
  if (controller[kState].pulling) {
    controller[kState].pullAgain = true;
    return;
  }
  assert(!controller[kState].pullAgain);
  controller[kState].pulling = true;
  if (controller[kState].pullFulfilled === undefined) {
    // The pull reaction closures only capture the controller, so they are
    // created once on the first pull and reused for every subsequent pull
    // instead of allocating two fresh closures per chunk.
    controller[kState].pullFulfilled = () => {
      controller[kState].pulling = false;
      if (controller[kState].pullAgain) {
        controller[kState].pullAgain = false;
        readableStreamDefaultControllerCallPullIfNeeded(controller);
      }
    };
    controller[kState].pullRejected =
      (error) => readableStreamDefaultControllerError(controller, error);
  }
  PromisePrototypeThen(
    controller[kState].pullAlgorithm(controller),
    controller[kState].pullFulfilled,
    controller[kState].pullRejected);
}

function readableStreamDefaultControllerClearAlgorithms(controller) {
  controller[kState].pullAlgorithm = undefined;
  controller[kState].cancelAlgorithm = undefined;
  controller[kState].sizeAlgorithm = undefined;
}

function readableStreamDefaultControllerError(controller, error) {
  const {
    stream,
  } = controller[kState];
  if (stream[kState].state === 'readable') {
    resetQueue(controller);
    readableStreamDefaultControllerClearAlgorithms(controller);
    readableStreamError(stream, error);
  }
}

function readableStreamDefaultControllerCancelSteps(controller, reason) {
  resetQueue(controller);
  const result = controller[kState].cancelAlgorithm(reason);
  readableStreamDefaultControllerClearAlgorithms(controller);
  return result;
}

function readableStreamDefaultControllerPullSteps(controller, readRequest) {
  const {
    stream,
    queue,
  } = controller[kState];
  if (queue.length) {
    const chunk = dequeueValue(controller);
    if (controller[kState].closeRequested && !queue.length) {
      readableStreamDefaultControllerClearAlgorithms(controller);
      readableStreamClose(stream);
    } else if (!controller[kState].closeRequested &&
               controller[kState].started &&
               controller[kState].highWaterMark -
                 controller[kState].queueTotalSize > 0) {
      // Reduced ShouldCallPull: the state is known to be readable and the
      // queue was non-empty, so no read requests can be parked.
      readableStreamDefaultControllerPull(controller);
    }
    readRequest[kChunk](chunk);
    return;
  }
  readableStreamAddReadRequest(stream, readRequest);
  // Reduced ShouldCallPull: the state is known to be readable, an empty
  // queue in that state implies close has not been requested (close with
  // an empty queue closes the stream immediately), and the read request
  // parked above already satisfies the reader-with-pending-reads arm.
  if (controller[kState].started)
    readableStreamDefaultControllerPull(controller);
}

function setupReadableStreamDefaultController(
  stream,
  controller,
  startAlgorithm,
  pullAlgorithm,
  cancelAlgorithm,
  highWaterMark,
  sizeAlgorithm) {
  assert(stream[kState].controller === undefined);
  controller[kState] = {
    cancelAlgorithm,
    closeRequested: false,
    highWaterMark,
    pullAgain: false,
    pullAlgorithm,
    pulling: false,
    pullFulfilled: undefined,
    pullRejected: undefined,
    queue: kEmptyQueue,
    queueTotalSize: 0,
    started: false,
    sizeAlgorithm,
    stream,
  };
  stream[kState].controller = controller;

  const startResult = startAlgorithm();

  if (startResult === null ||
      (typeof startResult !== 'object' && typeof startResult !== 'function')) {
    // Non-thenable start result: fulfillment is guaranteed and no .then
    // lookup on the result is observable, so run the post-start step
    // directly at the exact microtask position the promise reaction
    // would have had, skipping two promise allocations.
    queueMicrotask(() => {
      controller[kState].started = true;
      assert(!controller[kState].pulling);
      assert(!controller[kState].pullAgain);
      readableStreamDefaultControllerCallPullIfNeeded(controller);
    });
    return;
  }

  PromisePrototypeThen(
    new Promise((r) => r(startResult)),
    () => {
      controller[kState].started = true;
      assert(!controller[kState].pulling);
      assert(!controller[kState].pullAgain);
      readableStreamDefaultControllerCallPullIfNeeded(controller);
    },
    (error) => readableStreamDefaultControllerError(controller, error));
}

function setupReadableStreamDefaultControllerFromSource(
  stream,
  source,
  highWaterMark,
  sizeAlgorithm) {
  const controller = new ReadableStreamDefaultController(kSkipThrow);
  const start = source?.start;
  const pull = source?.pull;
  const cancel = source?.cancel;
  const startAlgorithm = start ?
    FunctionPrototypeBind(start, source, controller) :
    nonOpStart;
  const pullAlgorithm = pull ?
    createPromiseCallback1Param('source.pull', pull, source) :
    nonOpPull;
  const cancelAlgorithm = cancel ?
    createPromiseCallback1Param('source.cancel', cancel, source) :
    nonOpCancel;

  setupReadableStreamDefaultController(
    stream,
    controller,
    startAlgorithm,
    pullAlgorithm,
    cancelAlgorithm,
    highWaterMark,
    sizeAlgorithm);
}

function readableByteStreamControllerClose(controller) {
  const {
    closeRequested,
    pendingPullIntos,
    queueTotalSize,
    stream,
  } = controller[kState];

  if (closeRequested || stream[kState].state !== 'readable')
    return;

  if (queueTotalSize) {
    controller[kState].closeRequested = true;
    return;
  }

  if (pendingPullIntos.length) {
    const firstPendingPullInto = pendingPullIntos[0];
    if (firstPendingPullInto.bytesFilled % firstPendingPullInto.elementSize !== 0) {
      const error = new ERR_INVALID_STATE.TypeError('Partial read');
      readableByteStreamControllerError(controller, error);
      throw error;
    }
  }

  readableByteStreamControllerClearAlgorithms(controller);
  readableStreamClose(stream);
}

function readableByteStreamControllerCommitPullIntoDescriptor(stream, desc) {
  assert(stream[kState].state !== 'errored');
  assert(desc.type !== 'none');

  let done = false;
  if (stream[kState].state === 'closed') {
    assert(desc.bytesFilled % desc.elementSize === 0);
    done = true;
  }

  const filledView =
    readableByteStreamControllerConvertPullIntoDescriptor(desc);

  if (desc.type === 'default') {
    readableStreamFulfillReadRequest(stream, filledView, done);
  } else {
    assert(desc.type === 'byob');
    readableStreamFulfillReadIntoRequest(stream, filledView, done);
  }
}

function readableByteStreamControllerCommitPullIntoDescriptors(stream, descriptors) {
  for (let i = 0; i < descriptors.length; ++i) {
    readableByteStreamControllerCommitPullIntoDescriptor(
      stream,
      descriptors[i],
    );
  }
}

function readableByteStreamControllerInvalidateBYOBRequest(controller) {
  if (controller[kState].byobRequest === null)
    return;
  controller[kState].byobRequest[kState].controller = undefined;
  controller[kState].byobRequest[kState].view = null;
  controller[kState].byobRequest = null;
}

function readableByteStreamControllerClearAlgorithms(controller) {
  controller[kState].pullAlgorithm = undefined;
  controller[kState].cancelAlgorithm = undefined;
}

function readableByteStreamControllerClearPendingPullIntos(controller) {
  readableByteStreamControllerInvalidateBYOBRequest(controller);
  controller[kState].pendingPullIntos = [];
}

function readableByteStreamControllerGetDesiredSize(controller) {
  const {
    stream,
    highWaterMark,
    queueTotalSize,
  } = controller[kState];
  switch (stream[kState].state) {
    case 'errored': return null;
    case 'closed': return 0;
    default: return highWaterMark - queueTotalSize;
  }
}

function readableByteStreamControllerShouldCallPull(controller) {
  // Single-pass version of the spec's predicate chain (HasDefaultReader,
  // GetNumReadRequests, HasBYOBReader, GetNumReadIntoRequests,
  // GetDesiredSize): this runs at least once per chunk on every byte
  // stream path. The desired-size computation is inlined because the
  // stream state is already known to be 'readable' here.
  const controllerState = controller[kState];
  const stream = controllerState.stream;
  if (stream[kState].state !== 'readable' ||
      controllerState.closeRequested ||
      !controllerState.started) {
    return false;
  }
  const reader = stream[kState].reader;
  if (reader !== undefined && reader[kState] !== undefined) {
    const type = reader[kType];
    if (type === 'ReadableStreamDefaultReader') {
      if (reader[kState].readRequests.length) return true;
    } else if (type === 'ReadableStreamBYOBReader') {
      if (reader[kState].readIntoRequests.length) return true;
    }
  }

  return controllerState.highWaterMark - controllerState.queueTotalSize > 0;
}

function readableByteStreamControllerHandleQueueDrain(controller) {
  const {
    closeRequested,
    queueTotalSize,
    stream,
  } = controller[kState];
  assert(stream[kState].state === 'readable');
  if (!queueTotalSize && closeRequested) {
    readableByteStreamControllerClearAlgorithms(controller);
    readableStreamClose(stream);
    return;
  }
  readableByteStreamControllerCallPullIfNeeded(controller);
}

function readableByteStreamControllerPullInto(
  controller,
  view,
  min,
  readIntoRequest) {
  const {
    closeRequested,
    stream,
    pendingPullIntos,
  } = controller[kState];
  let elementSize = 1;
  let ctor = DataView;
  if (isArrayBufferView(view) && !isDataView(view)) {
    elementSize = view.constructor.BYTES_PER_ELEMENT;
    ctor = view.constructor;
  }

  const minimumFill = min * elementSize;
  assert(minimumFill >= elementSize && minimumFill <= view.byteLength);
  assert(minimumFill % elementSize === 0);

  const buffer = ArrayBufferViewGetBuffer(view);
  const byteOffset = ArrayBufferViewGetByteOffset(view);
  const byteLength = ArrayBufferViewGetByteLength(view);
  const bufferByteLength = ArrayBufferPrototypeGetByteLength(buffer);

  let transferredBuffer;
  try {
    transferredBuffer = ArrayBufferPrototypeTransfer(buffer);
  } catch (error) {
    readIntoRequest[kError](error);
    return;
  }
  const desc = {
    buffer: transferredBuffer,
    bufferByteLength,
    byteOffset,
    byteLength,
    bytesFilled: 0,
    minimumFill,
    elementSize,
    ctor,
    type: 'byob',
  };
  if (pendingPullIntos.length) {
    ArrayPrototypePush(pendingPullIntos, desc);
    readableStreamAddReadIntoRequest(stream, readIntoRequest);
    return;
  }
  if (stream[kState].state === 'closed') {
    const emptyView = new ctor(desc.buffer, byteOffset, 0);
    readIntoRequest[kClose](emptyView);
    return;
  }
  if (controller[kState].queueTotalSize) {
    if (readableByteStreamControllerFillPullIntoDescriptorFromQueue(
      controller,
      desc)) {
      const filledView =
        readableByteStreamControllerConvertPullIntoDescriptor(desc);
      readableByteStreamControllerHandleQueueDrain(controller);
      readIntoRequest[kChunk](filledView);
      return;
    }
    if (closeRequested) {
      const error = new ERR_INVALID_STATE.TypeError('ReadableStream closed');
      readableByteStreamControllerError(controller, error);
      readIntoRequest[kError](error);
      return;
    }
  }
  ArrayPrototypePush(pendingPullIntos, desc);
  readableStreamAddReadIntoRequest(stream, readIntoRequest);
  readableByteStreamControllerCallPullIfNeeded(controller);
}

function readableByteStreamControllerRespondInternal(controller, bytesWritten) {
  const {
    stream,
    pendingPullIntos,
  } = controller[kState];
  const desc = pendingPullIntos[0];
  readableByteStreamControllerInvalidateBYOBRequest(controller);
  if (stream[kState].state === 'closed') {
    if (bytesWritten)
      throw new ERR_INVALID_STATE.TypeError(
        'Controller is closed but view is not zero-length');
    readableByteStreamControllerRespondInClosedState(controller, desc);
  } else {
    assert(stream[kState].state === 'readable');
    if (!bytesWritten)
      throw new ERR_INVALID_STATE.TypeError('View cannot be zero-length');
    readableByteStreamControllerRespondInReadableState(
      controller,
      bytesWritten,
      desc);
  }
  readableByteStreamControllerCallPullIfNeeded(controller);
}

function readableByteStreamControllerRespond(controller, bytesWritten) {
  const {
    pendingPullIntos,
    stream,
  } = controller[kState];
  assert(pendingPullIntos.length);
  const desc = pendingPullIntos[0];

  if (stream[kState].state === 'closed') {
    if (bytesWritten !== 0)
      throw new ERR_INVALID_ARG_VALUE('bytesWritten', bytesWritten);
  } else {
    assert(stream[kState].state === 'readable');

    if (!bytesWritten)
      throw new ERR_INVALID_ARG_VALUE('bytesWritten', bytesWritten);

    if ((desc.bytesFilled + bytesWritten) > desc.byteLength)
      throw new ERR_INVALID_ARG_VALUE.RangeError('bytesWritten', bytesWritten);
  }

  desc.buffer = ArrayBufferPrototypeTransfer(desc.buffer);

  readableByteStreamControllerRespondInternal(controller, bytesWritten);
}

function readableByteStreamControllerRespondInClosedState(controller, desc) {
  assert(desc.bytesFilled % desc.elementSize === 0);
  if (desc.type === 'none') {
    readableByteStreamControllerShiftPendingPullInto(controller);
  }
  const {
    stream,
  } = controller[kState];
  if (readableStreamHasBYOBReader(stream)) {
    const filledPullIntos = [];
    for (let i = 0; i < readableStreamGetNumReadIntoRequests(stream); ++i) {
      ArrayPrototypePush(filledPullIntos, readableByteStreamControllerShiftPendingPullInto(controller));
    }
    readableByteStreamControllerCommitPullIntoDescriptors(stream, filledPullIntos);
  }
}

function readableByteStreamControllerFillHeadPullIntoDescriptor(
  controller,
  size,
  desc) {
  const {
    pendingPullIntos,
    byobRequest,
  } = controller[kState];
  assert(!pendingPullIntos.length || pendingPullIntos[0] === desc);
  assert(byobRequest === null);
  desc.bytesFilled += size;
}

function readableByteStreamControllerEnqueue(controller, chunk) {
  const {
    closeRequested,
    pendingPullIntos,
    queue,
    stream,
  } = controller[kState];

  const buffer = ArrayBufferViewGetBuffer(chunk);
  const byteOffset = ArrayBufferViewGetByteOffset(chunk);
  const byteLength = ArrayBufferViewGetByteLength(chunk);

  if (closeRequested || stream[kState].state !== 'readable')
    return;

  const transferredBuffer = ArrayBufferPrototypeTransfer(buffer);

  if (pendingPullIntos.length) {
    const firstPendingPullInto = pendingPullIntos[0];

    if (ArrayBufferPrototypeGetDetached(firstPendingPullInto.buffer)) {
      throw new ERR_INVALID_STATE.TypeError(
        'Destination ArrayBuffer is detached',
      );
    }

    readableByteStreamControllerInvalidateBYOBRequest(controller);

    firstPendingPullInto.buffer = ArrayBufferPrototypeTransfer(
      firstPendingPullInto.buffer,
    );

    if (firstPendingPullInto.type === 'none') {
      readableByteStreamControllerEnqueueDetachedPullIntoToQueue(
        controller,
        firstPendingPullInto,
      );
    }
  }

  // Single consolidated pass over the reader state. The spec routes this
  // through HasDefaultReader / ProcessReadRequestsUsingQueue /
  // GetNumReadRequests / FulfillReadRequest, which would re-run the same
  // reader brand check and re-load the read request list four times on
  // this per-chunk path.
  const { reader } = stream[kState];
  if (reader !== undefined &&
      reader[kState] !== undefined &&
      reader[kType] === 'ReadableStreamDefaultReader') {
    const { readRequests } = reader[kState];
    if (readRequests.length && controller[kState].queueTotalSize > 0) {
      // Only possible when data was enqueued while the stream was not
      // being read; read requests otherwise never coexist with a
      // non-empty queue.
      readableByteStreamControllerProcessReadRequestsUsingQueue(controller);
    }
    if (!readRequests.length) {
      readableByteStreamControllerEnqueueChunkToQueue(
        controller,
        transferredBuffer,
        byteOffset,
        byteLength);
    } else {
      assert(!queue.length);
      if (pendingPullIntos.length) {
        assert(pendingPullIntos[0].type === 'default');
        readableByteStreamControllerShiftPendingPullInto(controller);
      }
      const transferredView =
        new Uint8Array(transferredBuffer, byteOffset, byteLength);
      const readRequest = ArrayPrototypeShift(readRequests);
      readRequest[kChunk](transferredView);
    }
  } else if (readableStreamHasBYOBReader(stream)) {
    readableByteStreamControllerEnqueueChunkToQueue(
      controller,
      transferredBuffer,
      byteOffset,
      byteLength);
    const filledPullIntos = readableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(
      controller);
    readableByteStreamControllerCommitPullIntoDescriptors(stream, filledPullIntos);
  } else {
    assert(!isReadableStreamLocked(stream));
    readableByteStreamControllerEnqueueChunkToQueue(
      controller,
      transferredBuffer,
      byteOffset,
      byteLength);
  }
  readableByteStreamControllerCallPullIfNeeded(controller);
}

function readableByteStreamControllerEnqueueClonedChunkToQueue(
  controller,
  buffer,
  byteOffset,
  byteLength,
) {
  let cloneResult;
  try {
    cloneResult = ArrayBufferPrototypeSlice(
      buffer,
      byteOffset,
      byteOffset + byteLength,
    );
  } catch (error) {
    readableByteStreamControllerError(controller, error);
    throw error;
  }
  readableByteStreamControllerEnqueueChunkToQueue(
    controller,
    cloneResult,
    0,
    byteLength,
  );
}

function readableByteStreamControllerEnqueueChunkToQueue(
  controller,
  buffer,
  byteOffset,
  byteLength) {
  const state = controller[kState];
  materializeQueue(state).push({
    buffer,
    byteOffset,
    byteLength,
  });
  state.queueTotalSize += byteLength;
}

function readableByteStreamControllerEnqueueDetachedPullIntoToQueue(
  controller,
  desc,
) {
  const {
    buffer,
    byteOffset,
    bytesFilled,
    type,
  } = desc;
  assert(type === 'none');

  if (bytesFilled > 0) {
    readableByteStreamControllerEnqueueClonedChunkToQueue(
      controller,
      buffer,
      byteOffset,
      bytesFilled,
    );
  }
  readableByteStreamControllerShiftPendingPullInto(controller);
}

function readableByteStreamControllerFillPullIntoDescriptorFromQueue(
  controller,
  desc) {
  const {
    buffer,
    byteLength,
    byteOffset,
    bytesFilled,
    minimumFill,
    elementSize,
  } = desc;
  const maxBytesToCopy = MathMin(
    controller[kState].queueTotalSize,
    byteLength - bytesFilled);
  const maxBytesFilled = bytesFilled + maxBytesToCopy;
  const maxAlignedBytes = maxBytesFilled - (maxBytesFilled % elementSize);
  let totalBytesToCopyRemaining = maxBytesToCopy;
  let ready = false;
  assert(!ArrayBufferPrototypeGetDetached(buffer));
  assert(bytesFilled < minimumFill);
  if (maxAlignedBytes >= minimumFill) {
    totalBytesToCopyRemaining = maxAlignedBytes - bytesFilled;
    ready = true;
  }
  const {
    queue,
  } = controller[kState];

  while (totalBytesToCopyRemaining) {
    const headOfQueue = queue.peek();
    const bytesToCopy = MathMin(
      totalBytesToCopyRemaining,
      headOfQueue.byteLength);
    const destStart = byteOffset + desc.bytesFilled;
    assert(canCopyArrayBuffer(
      buffer,
      destStart,
      headOfQueue.buffer,
      headOfQueue.byteOffset,
      bytesToCopy));
    copyArrayBuffer(
      buffer,
      destStart,
      headOfQueue.buffer,
      headOfQueue.byteOffset,
      bytesToCopy);
    if (headOfQueue.byteLength === bytesToCopy) {
      queue.shift();
    } else {
      headOfQueue.byteOffset += bytesToCopy;
      headOfQueue.byteLength -= bytesToCopy;
    }
    controller[kState].queueTotalSize -= bytesToCopy;
    readableByteStreamControllerFillHeadPullIntoDescriptor(
      controller,
      bytesToCopy,
      desc);
    totalBytesToCopyRemaining -= bytesToCopy;
  }

  if (!ready) {
    assert(!controller[kState].queueTotalSize);
    assert(desc.bytesFilled > 0);
    assert(desc.bytesFilled < minimumFill);
  }
  return ready;
}

function readableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(
  controller) {
  const {
    closeRequested,
    pendingPullIntos,
  } = controller[kState];
  assert(!closeRequested);
  const filledPullIntos = [];
  while (pendingPullIntos.length) {
    if (!controller[kState].queueTotalSize)
      break;
    const desc = pendingPullIntos[0];
    if (readableByteStreamControllerFillPullIntoDescriptorFromQueue(
      controller,
      desc)) {
      readableByteStreamControllerShiftPendingPullInto(controller);
      ArrayPrototypePush(filledPullIntos, desc);
    }
  }
  return filledPullIntos;
}

function readableByteStreamControllerRespondInReadableState(
  controller,
  bytesWritten,
  desc) {
  const {
    stream,
  } = controller[kState];
  const {
    buffer,
    bytesFilled,
    byteLength,
    type,
  } = desc;

  if (bytesFilled + bytesWritten > byteLength)
    throw new ERR_INVALID_STATE.RangeError('The buffer size is invalid');

  readableByteStreamControllerFillHeadPullIntoDescriptor(
    controller,
    bytesWritten,
    desc);

  if (type === 'none') {
    readableByteStreamControllerEnqueueDetachedPullIntoToQueue(
      controller,
      desc,
    );
    const filledPullIntos = readableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(
      controller,
    );
    readableByteStreamControllerCommitPullIntoDescriptors(stream, filledPullIntos);
    return;
  }

  if (desc.bytesFilled < desc.minimumFill)
    return;

  readableByteStreamControllerShiftPendingPullInto(controller);

  const remainderSize = desc.bytesFilled % desc.elementSize;

  if (remainderSize) {
    const end = desc.byteOffset + desc.bytesFilled;
    const start = end - remainderSize;
    const remainder =
      ArrayBufferPrototypeSlice(
        buffer,
        start,
        end);
    readableByteStreamControllerEnqueueChunkToQueue(
      controller,
      remainder,
      0,
      ArrayBufferPrototypeGetByteLength(remainder));
  }
  desc.bytesFilled -= remainderSize;
  const filledPullIntos = readableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller);

  readableByteStreamControllerCommitPullIntoDescriptor(stream, desc);
  readableByteStreamControllerCommitPullIntoDescriptors(stream, filledPullIntos);
}

function readableByteStreamControllerRespondWithNewView(controller, view) {
  const {
    stream,
    pendingPullIntos,
  } = controller[kState];
  assert(pendingPullIntos.length);

  const desc = pendingPullIntos[0];
  assert(stream[kState].state !== 'errored');

  const viewByteLength = ArrayBufferViewGetByteLength(view);
  const viewByteOffset = ArrayBufferViewGetByteOffset(view);
  const viewBuffer = ArrayBufferViewGetBuffer(view);
  const viewBufferByteLength = ArrayBufferPrototypeGetByteLength(viewBuffer);

  if (stream[kState].state === 'closed') {
    if (viewByteLength !== 0)
      throw new ERR_INVALID_STATE.TypeError('View is not zero-length');
  } else {
    assert(stream[kState].state === 'readable');
    if (viewByteLength === 0)
      throw new ERR_INVALID_STATE.TypeError('View is zero-length');
  }

  const {
    byteOffset,
    byteLength,
    bytesFilled,
    bufferByteLength,
  } = desc;

  if (byteOffset + bytesFilled !== viewByteOffset)
    throw new ERR_INVALID_ARG_VALUE.RangeError('view', view);

  if (bytesFilled + viewByteLength > byteLength)
    throw new ERR_INVALID_ARG_VALUE.RangeError('view', view);

  if (bufferByteLength !== viewBufferByteLength)
    throw new ERR_INVALID_ARG_VALUE.RangeError('view', view);

  desc.buffer = ArrayBufferPrototypeTransfer(viewBuffer);

  readableByteStreamControllerRespondInternal(controller, viewByteLength);
}

function readableByteStreamControllerShiftPendingPullInto(controller) {
  assert(controller[kState].byobRequest === null);
  return ArrayPrototypeShift(controller[kState].pendingPullIntos);
}

function readableByteStreamControllerCallPullIfNeeded(controller) {
  if (!readableByteStreamControllerShouldCallPull(controller))
    return;
  if (controller[kState].pulling) {
    controller[kState].pullAgain = true;
    return;
  }
  assert(!controller[kState].pullAgain);
  controller[kState].pulling = true;
  if (controller[kState].pullFulfilled === undefined) {
    // See readableStreamDefaultControllerCallPullIfNeeded: created once,
    // reused for every pull.
    controller[kState].pullFulfilled = () => {
      controller[kState].pulling = false;
      if (controller[kState].pullAgain) {
        controller[kState].pullAgain = false;
        readableByteStreamControllerCallPullIfNeeded(controller);
      }
    };
    controller[kState].pullRejected =
      (error) => readableByteStreamControllerError(controller, error);
  }
  PromisePrototypeThen(
    controller[kState].pullAlgorithm(controller),
    controller[kState].pullFulfilled,
    controller[kState].pullRejected);
}

function readableByteStreamControllerError(controller, error) {
  const {
    stream,
  } = controller[kState];
  if (stream[kState].state !== 'readable')
    return;
  readableByteStreamControllerClearPendingPullIntos(controller);
  resetQueue(controller);
  readableByteStreamControllerClearAlgorithms(controller);
  readableStreamError(stream, error);
}

function readableByteStreamControllerCancelSteps(controller, reason) {
  readableByteStreamControllerClearPendingPullIntos(controller);
  resetQueue(controller);
  const result = controller[kState].cancelAlgorithm(reason);
  readableByteStreamControllerClearAlgorithms(controller);
  return result;
}

// Dequeues the first chunk of the byte queue as a Uint8Array view,
// handling queue drain (close-on-empty or pull) before the view is
// created. This is the [[queueTotalSize]] > 0 arm of the byte
// controller's pull steps; it is also called directly from the
// buffered fast paths in ReadableStreamDefaultReader.read() and the
// async iterator, which resolve with the view without allocating a
// read request.
function readableByteStreamControllerDequeueChunk(controller) {
  assert(controller[kState].queueTotalSize > 0);
  const {
    buffer,
    byteOffset,
    byteLength,
  } = controller[kState].queue.shift();

  controller[kState].queueTotalSize -= byteLength;
  readableByteStreamControllerHandleQueueDrain(controller);
  return new Uint8Array(buffer, byteOffset, byteLength);
}

function readableByteStreamControllerFillReadRequestFromQueue(controller, readRequest) {
  readRequest[kChunk](readableByteStreamControllerDequeueChunk(controller));
}

function readableByteStreamControllerProcessReadRequestsUsingQueue(controller) {
  const {
    stream,
    queueTotalSize,
  } = controller[kState];
  const { reader } = stream[kState];
  assert(isReadableStreamDefaultReader(reader));

  while (reader[kState].readRequests.length > 0) {
    if (queueTotalSize === 0) {
      return;
    }
    readableByteStreamControllerFillReadRequestFromQueue(
      controller,
      ArrayPrototypeShift(reader[kState].readRequests),
    );
  }
}

function readableByteStreamControllerPullSteps(controller, readRequest) {
  const {
    pendingPullIntos,
    queueTotalSize,
    stream,
  } = controller[kState];
  assert(readableStreamHasDefaultReader(stream));
  if (queueTotalSize) {
    assert(!readableStreamGetNumReadRequests(stream));
    readableByteStreamControllerFillReadRequestFromQueue(
      controller,
      readRequest,
    );
    return;
  }
  const {
    autoAllocateChunkSize,
  } = controller[kState];
  if (autoAllocateChunkSize !== undefined) {
    try {
      const buffer = new ArrayBuffer(autoAllocateChunkSize);
      ArrayPrototypePush(
        pendingPullIntos,
        {
          buffer,
          bufferByteLength: autoAllocateChunkSize,
          byteOffset: 0,
          byteLength: autoAllocateChunkSize,
          bytesFilled: 0,
          minimumFill: 1,
          elementSize: 1,
          ctor: Uint8Array,
          type: 'default',
        });
    } catch (error) {
      readRequest[kError](error);
      return;
    }
  }

  readableStreamAddReadRequest(stream, readRequest);
  readableByteStreamControllerCallPullIfNeeded(controller);
}

function setupReadableByteStreamController(
  stream,
  controller,
  startAlgorithm,
  pullAlgorithm,
  cancelAlgorithm,
  highWaterMark,
  autoAllocateChunkSize) {
  assert(stream[kState].controller === undefined);
  if (autoAllocateChunkSize !== undefined) {
    assert(NumberIsInteger(autoAllocateChunkSize));
    assert(autoAllocateChunkSize > 0);
  }
  controller[kState] = {
    byobRequest: null,
    closeRequested: false,
    pullAgain: false,
    pulling: false,
    pullFulfilled: undefined,
    pullRejected: undefined,
    started: false,
    stream,
    queue: kEmptyQueue,
    queueTotalSize: 0,
    highWaterMark,
    pullAlgorithm,
    cancelAlgorithm,
    autoAllocateChunkSize,
    pendingPullIntos: [],
  };
  stream[kState].controller = controller;

  const startResult = startAlgorithm();

  if (startResult === null ||
      (typeof startResult !== 'object' && typeof startResult !== 'function')) {
    // See setupReadableStreamDefaultController.
    queueMicrotask(() => {
      controller[kState].started = true;
      assert(!controller[kState].pulling);
      assert(!controller[kState].pullAgain);
      readableByteStreamControllerCallPullIfNeeded(controller);
    });
    return;
  }

  PromisePrototypeThen(
    new Promise((r) => r(startResult)),
    () => {
      controller[kState].started = true;
      assert(!controller[kState].pulling);
      assert(!controller[kState].pullAgain);
      readableByteStreamControllerCallPullIfNeeded(controller);
    },
    (error) => readableByteStreamControllerError(controller, error));
}

function setupReadableByteStreamControllerFromSource(
  stream,
  source,
  highWaterMark) {
  const controller = new ReadableByteStreamController(kSkipThrow);
  const start = source?.start;
  const pull = source?.pull;
  const cancel = source?.cancel;
  const autoAllocateChunkSize = source?.autoAllocateChunkSize;
  const startAlgorithm = start ?
    FunctionPrototypeBind(start, source, controller) :
    nonOpStart;
  const pullAlgorithm = pull ?
    createPromiseCallback1Param('source.pull', pull, source) :
    nonOpPull;
  const cancelAlgorithm = cancel ?
    createPromiseCallback1Param('source.cancel', cancel, source) :
    nonOpCancel;

  if (autoAllocateChunkSize === 0) {
    throw new ERR_INVALID_ARG_VALUE(
      'source.autoAllocateChunkSize',
      autoAllocateChunkSize);
  }
  setupReadableByteStreamController(
    stream,
    controller,
    startAlgorithm,
    pullAlgorithm,
    cancelAlgorithm,
    highWaterMark,
    autoAllocateChunkSize);
}

module.exports = {
  ReadableStream,
  ReadableStreamDefaultReader,
  ReadableStreamBYOBReader,
  ReadableStreamBYOBRequest,
  ReadableByteStreamController,
  ReadableStreamDefaultController,
  TransferredReadableStream,

  // Exported Brand Checks
  isReadableStream,
  isReadableByteStreamController,
  isReadableStreamBYOBRequest,
  isReadableStreamDefaultReader,
  isReadableStreamBYOBReader,
  isWritableStreamDefaultWriter,
  isWritableStreamDefaultController,

  readableStreamPipeTo,
  readableStreamTee,
  readableByteStreamControllerConvertPullIntoDescriptor,
  isReadableStreamLocked,
  readableStreamCancel,
  readableStreamClose,
  readableStreamError,
  readableStreamHasDefaultReader,
  readableStreamGetNumReadRequests,
  readableStreamHasBYOBReader,
  readableStreamGetNumReadIntoRequests,
  readableStreamFulfillReadRequest,
  readableStreamFulfillReadIntoRequest,
  readableStreamAddReadRequest,
  readableStreamAddReadIntoRequest,
  readableStreamReaderGenericCancel,
  readableStreamReaderGenericInitialize,
  readableStreamReaderGenericRelease,
  readableStreamBYOBReaderRead,
  readableStreamDefaultReaderRead,
  setupReadableStreamBYOBReader,
  setupReadableStreamDefaultReader,
  readableStreamDefaultControllerClose,
  readableStreamDefaultControllerEnqueue,
  readableStreamDefaultControllerHasBackpressure,
  readableStreamDefaultControllerCanCloseOrEnqueue,
  readableStreamDefaultControllerGetDesiredSize,
  readableStreamDefaultControllerShouldCallPull,
  readableStreamDefaultControllerCallPullIfNeeded,
  readableStreamDefaultControllerClearAlgorithms,
  readableStreamDefaultControllerError,
  readableStreamDefaultControllerCancelSteps,
  readableStreamDefaultControllerPullSteps,
  setupReadableStreamDefaultController,
  setupReadableStreamDefaultControllerFromSource,
  readableByteStreamControllerClose,
  readableByteStreamControllerCommitPullIntoDescriptor,
  readableByteStreamControllerInvalidateBYOBRequest,
  readableByteStreamControllerClearAlgorithms,
  readableByteStreamControllerClearPendingPullIntos,
  readableByteStreamControllerGetDesiredSize,
  readableByteStreamControllerShouldCallPull,
  readableByteStreamControllerHandleQueueDrain,
  readableByteStreamControllerPullInto,
  readableByteStreamControllerRespondInternal,
  readableByteStreamControllerRespond,
  readableByteStreamControllerRespondInClosedState,
  readableByteStreamControllerFillHeadPullIntoDescriptor,
  readableByteStreamControllerEnqueue,
  readableByteStreamControllerEnqueueChunkToQueue,
  readableByteStreamControllerFillPullIntoDescriptorFromQueue,
  readableByteStreamControllerProcessPullIntoDescriptorsUsingQueue,
  readableByteStreamControllerRespondInReadableState,
  readableByteStreamControllerRespondWithNewView,
  readableByteStreamControllerShiftPendingPullInto,
  readableByteStreamControllerCallPullIfNeeded,
  readableByteStreamControllerError,
  readableByteStreamControllerCancelSteps,
  readableByteStreamControllerPullSteps,
  setupReadableByteStreamController,
  setupReadableByteStreamControllerFromSource,
  createReadableStream,
  createReadableByteStream,
};
