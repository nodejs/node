'use strict';

const {
  ArrayPrototypePush,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectPrototypeIsPrototypeOf,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  ReflectApply,
  SafePromiseAll,
  Symbol,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
  SymbolToStringTag,
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
  },
} = require('internal/errors');

const {
  DOMException,
} = internalBinding('messaging');

const {
  markPromiseAsHandled,
} = internalBinding('util');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
} = require('internal/util');

const {
  validateAbortSignal,
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
  AsyncIterator,
  cloneAsUint8Array,
  createPromiseCallback,
  customInspect,
  extractHighWaterMark,
  extractSizeMode,
  getNonWritablePropertyDescriptor,
  lazyTransfer,
  nonOpCancel,
  nonOpPull,
  nonOpStart,
  setPromiseHandled,
} = require('internal/webstreams/util');

const {
  ReadableStream: ReadableStreamNative,
  ReadableStreamDefaultReader: ReadableStreamDefaultReaderNative,
  ReadableStreamBYOBReader: ReadableStreamBYOBReaderNative,
  ReadableStreamBYOBRequest: ReadableStreamBYOBRequestNative,
  ReadableStreamDefaultController: ReadableStreamDefaultControllerNative,
  ReadableByteStreamController: ReadableByteStreamControllerNative,
  createReadableStream: createReadableStreamNative,
  createReadableByteStream: createReadableByteStreamNative,
  acquireReadableStreamDefaultReader,
  acquireReadableStreamBYOBReader,
  readableStreamStateField,
  readableStreamDisturbed,
  readableStreamClosedPromise,
  writableStreamStateField,
  writableStreamCloseQueuedOrInFlight,
} = internalBinding('webstreams');

const {
  WritableStreamDefaultWriter,

  isWritableStream,
  isWritableStreamLocked,
} = require('internal/webstreams/writablestream');

const assert = require('internal/assert');

let addAbortListener;

// Mirrors the C++ ReadableState enum (src/streams/readable_stream.h):
//   0 = readable, 1 = closed, 2 = errored
const kReadableStateNames = ['readable', 'closed', 'errored'];

// Deferred cross-realm source for a deserialized stream; its port is bound once
// it becomes available in [kDeserialize].
const kCrossRealmSource = Symbol('kCrossRealmSource');
// Per-stream transfer bookkeeping (cross-realm ports + pipe promise), kept as a
// private symbol on the native stream object rather than as JS wrapper state.
const kTransferState = Symbol('kTransferState');
// Records whether a stream was built with a byte controller, for inspect's
// `supportsBYOB`. Native byte controllers do not currently expose a brand check
// that survives the thin-wrapper model, so we stamp it at construction time.
const kIsByteStream = Symbol('kIsByteStream');

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

const isReadableStream = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(ReadableStreamNative.prototype, value);
const isReadableStreamDefaultReader = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(ReadableStreamDefaultReaderNative.prototype,
                              value);
const isReadableStreamBYOBReader = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(ReadableStreamBYOBReaderNative.prototype, value);
const isReadableStreamBYOBRequest = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(ReadableStreamBYOBRequestNative.prototype,
                              value);
const isReadableStreamDefaultController = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(ReadableStreamDefaultControllerNative.prototype,
                              value);
const isReadableByteStreamController = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(ReadableByteStreamControllerNative.prototype,
                              value);

function isReadableStreamLocked(stream) {
  return stream.locked;
}

// Property-descriptor factories. Native prototype members (locked/cancel/read
// /releaseLock/etc.) are already enumerable+configurable, so we only graft the
// JS-only additions and never redefine native accessors.
const methodProp = (value) => ({
  __proto__: null, configurable: true, enumerable: true, writable: true, value,
});
const hiddenMethodProp = (value) => ({
  __proto__: null, configurable: true, enumerable: false, writable: true, value,
});
const ctorProp = (value) => ({
  __proto__: null, configurable: true, enumerable: false, writable: true, value,
});

// Core creation path for a default-controller ReadableStream: build the native
// stream from already-wrapped algorithm functions, then apply the JS-only
// decorations (transfer mode + node:stream interop error hook). The controller
// is captured from the start algorithm (run synchronously by the native setup)
// so the interop layer can error the stream via it.
function newReadableStream(startAlgorithm, pullAlgorithm, cancelAlgorithm,
                          highWaterMark, sizeMode, sizeFn) {
  let controller;
  const wrappedStart = (c) => {
    controller = c;
    return startAlgorithm(c);
  };
  const stream = createReadableStreamNative(
    wrappedStart,
    pullAlgorithm,
    cancelAlgorithm,
    highWaterMark,
    sizeMode,
    sizeFn);
  markTransferMode(stream, false, true);
  stream[kControllerErrorFunction] = (error) => controller.error(error);
  stream[kIsByteStream] = false;
  return stream;
}

// Core creation path for a byte ReadableStream.
function newReadableByteStream(startAlgorithm, pullAlgorithm, cancelAlgorithm,
                              highWaterMark, autoAllocateChunkSize) {
  let controller;
  const wrappedStart = (c) => {
    controller = c;
    return startAlgorithm(c);
  };
  const stream = createReadableByteStreamNative(
    wrappedStart,
    pullAlgorithm,
    cancelAlgorithm,
    highWaterMark,
    autoAllocateChunkSize);
  markTransferMode(stream, false, true);
  stream[kControllerErrorFunction] = (error) => controller.error(error);
  stream[kIsByteStream] = true;
  return stream;
}

// Builds the algorithm closures from an underlying source dictionary and
// produces a default-controller stream.
function newReadableStreamFromSource(source, highWaterMark, sizeMode, sizeFn) {
  const start = source?.start;
  const pull = source?.pull;
  const cancel = source?.cancel;
  const startAlgorithm = start !== undefined ?
    (controller) => ReflectApply(start, source, [controller]) :
    nonOpStart;
  const pullAlgorithm = pull !== undefined ?
    createPromiseCallback('source.pull', pull, source) :
    nonOpPull;
  const cancelAlgorithm = cancel !== undefined ?
    createPromiseCallback('source.cancel', cancel, source) :
    nonOpCancel;
  return newReadableStream(startAlgorithm, pullAlgorithm, cancelAlgorithm,
                          highWaterMark, sizeMode, sizeFn);
}

// Builds the algorithm closures from a byte underlying source dictionary.
function newReadableByteStreamFromSource(source, highWaterMark) {
  const start = source?.start;
  const pull = source?.pull;
  const cancel = source?.cancel;
  const autoAllocateChunkSize = source?.autoAllocateChunkSize;
  const startAlgorithm = start !== undefined ?
    (controller) => ReflectApply(start, source, [controller]) :
    nonOpStart;
  const pullAlgorithm = pull !== undefined ?
    createPromiseCallback('source.pull', pull, source) :
    nonOpPull;
  const cancelAlgorithm = cancel !== undefined ?
    createPromiseCallback('source.cancel', cancel, source) :
    nonOpCancel;

  if (autoAllocateChunkSize === 0) {
    throw new ERR_INVALID_ARG_VALUE(
      'source.autoAllocateChunkSize',
      autoAllocateChunkSize);
  }
  // 0 signals "none" to the native factory; undefined maps to 0.
  return newReadableByteStream(
    startAlgorithm,
    pullAlgorithm,
    cancelAlgorithm,
    highWaterMark,
    autoAllocateChunkSize ?? 0);
}

/**
 * The public ReadableStream interface. The returned object is the C++
 * ReadableStream; all per-operation logic lives in C++.
 */
function ReadableStream(source = kEmptyObject, strategy = kEmptyObject) {
  if (new.target === undefined)
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  validateObject(source, 'source', kValidateObjectAllowObjects);
  validateObject(strategy, 'strategy', kValidateObjectAllowObjectsAndNull);

  // The spec requires handling of the strategy first here. Specifically, if
  // getting the size and highWaterMark from the strategy fail, that has to
  // trigger a throw before getting the details from the source. So be sure to
  // keep these in this order.
  const size = strategy?.size;
  const highWaterMark = strategy?.highWaterMark;
  const type = source.type;

  let stream;
  if (`${type}` === 'bytes') {
    if (size !== undefined)
      throw new ERR_INVALID_ARG_VALUE.RangeError('strategy.size', size);
    stream = newReadableByteStreamFromSource(
      source,
      extractHighWaterMark(highWaterMark, 0));
  } else {
    if (type !== undefined)
      throw new ERR_INVALID_ARG_VALUE('source.type', type);
    const { mode, size: sizeFn } = extractSizeMode(size);
    stream = newReadableStreamFromSource(
      source,
      extractHighWaterMark(highWaterMark, 1),
      mode,
      sizeFn);
  }
  // Support subclassing: the native object's prototype must reflect new.target
  // (the constructor otherwise returns an object with ReadableStream.prototype).
  if (new.target !== ReadableStream)
    ObjectSetPrototypeOf(stream, new.target.prototype);
  return stream;
}
ObjectDefineProperty(ReadableStream, 'prototype', {
  __proto__: null,
  value: ReadableStreamNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function from(iterable) {
  return readableStreamFromIterable(iterable);
}

function getReader(options = kEmptyObject) {
  if (!isReadableStream(this))
    throw new ERR_INVALID_THIS('ReadableStream');
  validateObject(options, 'options', kValidateObjectAllowObjectsAndNull);
  const mode = options?.mode;

  if (mode === undefined)
    return new ReadableStreamDefaultReader(this);

  if (`${mode}` !== 'byob')
    throw new ERR_INVALID_ARG_VALUE('options.mode', mode);
  return new ReadableStreamBYOBReader(this);
}

/**
 * @param {ReadableWritablePair} transform
 * @param {StreamPipeOptions} [options]
 * @returns {ReadableStream}
 */
function pipeThrough(transform, options = kEmptyObject) {
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
function pipeTo(destination, options = kEmptyObject) {
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
function tee() {
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
function values(options = kEmptyObject) {
  if (!isReadableStream(this))
    throw new ERR_INVALID_THIS('ReadableStream');
  validateObject(options, 'options', kValidateObjectAllowObjectsAndNull);
  const preventCancel = !!(options?.preventCancel);

  const reader = new ReadableStreamDefaultReader(this);

  // No __proto__ here to avoid the performance hit.
  const state = {
    done: false,
    current: undefined,
  };
  let started = false;

  // The nextSteps function is not an async function in order
  // to make it more efficient. Because nextSteps explicitly
  // creates a Promise and returns it in the common case,
  // making it an async function just causes two additional
  // unnecessary Promise allocations to occur, which just add
  // cost.
  function nextSteps() {
    if (state.done)
      return PromiseResolve({ done: true, value: undefined });

    return PromisePrototypeThen(
      reader.read(),
      (result) => {
        if (result.done) {
          state.done = true;
          reader.releaseLock();
          return { done: true, value: undefined };
        }
        return { done: false, value: result.value };
      },
      (error) => {
        state.done = true;
        reader.releaseLock();
        throw error;
      });
  }

  async function returnSteps(value) {
    if (state.done)
      return { done: true, value }; // eslint-disable-line node-core/avoid-prototype-pollution
    state.done = true;

    if (!preventCancel) {
      const result = reader.cancel(value);
      reader.releaseLock();
      await result;
      return { done: true, value }; // eslint-disable-line node-core/avoid-prototype-pollution
    }

    reader.releaseLock();
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
      state.current = state.current !== undefined ?
        PromisePrototypeThen(state.current, nextSteps, nextSteps) :
        nextSteps();
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

function readableStreamInspect(depth, options) {
  return customInspect(depth, options, 'ReadableStream', {
    locked: this.locked,
    state: kReadableStateNames[readableStreamStateField(this)],
    supportsBYOB: this[kIsByteStream] === true,
  });
}

function readableStreamTransfer() {
  if (!isReadableStream(this))
    throw new ERR_INVALID_THIS('ReadableStream');
  if (this.locked) {
    this[kTransferState]?.port1?.close();
    this[kTransferState] = undefined;
    throw new DOMException(
      'Cannot transfer a locked ReadableStream',
      'DataCloneError');
  }

  const transferState = this[kTransferState];
  const {
    writable,
    promise,
  } = lazyTransfer().newCrossRealmWritableSink(this, transferState.port1);

  transferState.writable = writable;
  transferState.promise = promise;

  return {
    data: { port: transferState.port2 },
    deserializeInfo:
      'internal/webstreams/readablestream:TransferredReadableStream',
  };
}

function readableStreamTransferList() {
  const { port1, port2 } = new MessageChannel();
  this[kTransferState] = {
    __proto__: null,
    port1,
    port2,
    writable: undefined,
    promise: undefined,
  };
  return [port2];
}

function readableStreamDeserialize({ port }) {
  // `this` is the native ReadableStream from TransferredReadableStream(); bind
  // the now-available port into its deferred cross-realm source.
  const transfer = lazyTransfer();
  this[kCrossRealmSource]?.[transfer.kBindPort](port);
}

ObjectDefineProperties(ReadableStream.prototype, {
  constructor: ctorProp(ReadableStream),
  getReader: methodProp(getReader),
  pipeThrough: methodProp(pipeThrough),
  pipeTo: methodProp(pipeTo),
  tee: methodProp(tee),
  values: methodProp(values),
  [SymbolAsyncIterator]: methodProp(values),
  [kInspect]: hiddenMethodProp(readableStreamInspect),
  [kTransfer]: hiddenMethodProp(readableStreamTransfer),
  [kTransferList]: hiddenMethodProp(readableStreamTransferList),
  [kDeserialize]: hiddenMethodProp(readableStreamDeserialize),
  [kIsDisturbed]: { __proto__: null, configurable: true, enumerable: false,
                    get() { return readableStreamDisturbed(this); } },
  [kIsErrored]: { __proto__: null, configurable: true, enumerable: false,
                  get() { return readableStreamStateField(this) === 2; } },
  [kIsReadable]: { __proto__: null, configurable: true, enumerable: false,
                   get() { return readableStreamStateField(this) === 0; } },
  [kIsClosedPromise]: { __proto__: null, configurable: true, enumerable: false,
                        get() {
                          return { promise: readableStreamClosedPromise(this) };
                        } },
  [SymbolToStringTag]: getNonWritablePropertyDescriptor('ReadableStream'),
});

ObjectDefineProperty(ReadableStream, 'from', methodProp(from));

// Produces a native ReadableStream wired to a deferred cross-realm source; the
// deserializeInfo target for a transferred ReadableStream.
function TransferredReadableStream() {
  const transfer = lazyTransfer();
  // The MessagePort arrives later via [kDeserialize]; create the source in
  // deferred-port mode (port === undefined) and bind it then.
  const source = new transfer.CrossRealmTransformReadableSource(undefined, true);
  const stream = newReadableStreamFromSource(source, 0, 0, undefined);
  stream[kCrossRealmSource] = source;
  return stream;
}
ObjectDefineProperty(TransferredReadableStream, 'prototype', {
  __proto__: null,
  value: ReadableStream.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

/**
 * The public ReadableStreamDefaultReader interface. Acquiring a reader builds
 * the C++ reader (which locks the stream).
 */
function ReadableStreamDefaultReader(stream) {
  if (new.target === undefined)
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  if (!isReadableStream(stream))
    throw new ERR_INVALID_ARG_TYPE('stream', 'ReadableStream', stream);
  return acquireReadableStreamDefaultReader(stream);
}
ObjectDefineProperty(ReadableStreamDefaultReader, 'prototype', {
  __proto__: null,
  value: ReadableStreamDefaultReaderNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function defaultReaderInspect(depth, options) {
  return customInspect(depth, options, 'ReadableStreamDefaultReader', {
    closed: this.closed,
  });
}

ObjectDefineProperties(ReadableStreamDefaultReader.prototype, {
  constructor: ctorProp(ReadableStreamDefaultReader),
  [kInspect]: hiddenMethodProp(defaultReaderInspect),
  [SymbolToStringTag]:
    getNonWritablePropertyDescriptor('ReadableStreamDefaultReader'),
});

/**
 * The public ReadableStreamBYOBReader interface. Acquiring a reader builds the
 * C++ reader (which locks the stream and requires a byte stream).
 */
function ReadableStreamBYOBReader(stream) {
  if (new.target === undefined)
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  if (!isReadableStream(stream))
    throw new ERR_INVALID_ARG_TYPE('stream', 'ReadableStream', stream);
  return acquireReadableStreamBYOBReader(stream);
}
ObjectDefineProperty(ReadableStreamBYOBReader, 'prototype', {
  __proto__: null,
  value: ReadableStreamBYOBReaderNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function byobReaderInspect(depth, options) {
  return customInspect(depth, options, 'ReadableStreamBYOBReader', {
    closed: this.closed,
  });
}

ObjectDefineProperties(ReadableStreamBYOBReader.prototype, {
  constructor: ctorProp(ReadableStreamBYOBReader),
  [kInspect]: hiddenMethodProp(byobReaderInspect),
  [SymbolToStringTag]:
    getNonWritablePropertyDescriptor('ReadableStreamBYOBReader'),
});

/**
 * The public ReadableStreamBYOBRequest interface. Not user-constructible.
 */
function ReadableStreamBYOBRequest() {
  throw new ERR_ILLEGAL_CONSTRUCTOR();
}
ObjectDefineProperty(ReadableStreamBYOBRequest, 'prototype', {
  __proto__: null,
  value: ReadableStreamBYOBRequestNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function byobRequestInspect(depth, options) {
  return customInspect(depth, options, 'ReadableStreamBYOBRequest', {
    view: this.view,
  });
}

ObjectDefineProperties(ReadableStreamBYOBRequest.prototype, {
  constructor: ctorProp(ReadableStreamBYOBRequest),
  [kInspect]: hiddenMethodProp(byobRequestInspect),
  [SymbolToStringTag]:
    getNonWritablePropertyDescriptor('ReadableStreamBYOBRequest'),
});

/**
 * The public ReadableStreamDefaultController interface. Not user-constructible.
 */
function ReadableStreamDefaultController() {
  throw new ERR_ILLEGAL_CONSTRUCTOR();
}
ObjectDefineProperty(ReadableStreamDefaultController, 'prototype', {
  __proto__: null,
  value: ReadableStreamDefaultControllerNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function defaultControllerInspect(depth, options) {
  return customInspect(depth, options, 'ReadableStreamDefaultController', {});
}

ObjectDefineProperties(ReadableStreamDefaultController.prototype, {
  constructor: ctorProp(ReadableStreamDefaultController),
  [kInspect]: hiddenMethodProp(defaultControllerInspect),
  [SymbolToStringTag]:
    getNonWritablePropertyDescriptor('ReadableStreamDefaultController'),
});

/**
 * The public ReadableByteStreamController interface. Not user-constructible.
 */
function ReadableByteStreamController() {
  throw new ERR_ILLEGAL_CONSTRUCTOR();
}
ObjectDefineProperty(ReadableByteStreamController, 'prototype', {
  __proto__: null,
  value: ReadableByteStreamControllerNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function byteControllerInspect(depth, options) {
  return customInspect(depth, options, 'ReadableByteStreamController', {});
}

ObjectDefineProperties(ReadableByteStreamController.prototype, {
  constructor: ctorProp(ReadableByteStreamController),
  [kInspect]: hiddenMethodProp(byteControllerInspect),
  [SymbolToStringTag]:
    getNonWritablePropertyDescriptor('ReadableByteStreamController'),
});

// ---- Internal factories (mirror the old createReadableStream signatures) ----

// Internal factory mirroring the old createReadableStream signature, used by
// internal callers that build a stream from raw algorithms.
function createReadableStream(start, pull, cancel,
                             highWaterMark = 1, size = () => 1) {
  const { mode, size: sizeFn } = extractSizeMode(size);
  return newReadableStream(
    start ?? nonOpStart,
    pull ?? nonOpPull,
    cancel ?? nonOpCancel,
    highWaterMark,
    mode,
    sizeFn);
}

// Internal factory mirroring the old createReadableByteStream signature.
function createReadableByteStream(start, pull, cancel) {
  return newReadableByteStream(
    start ?? nonOpStart,
    pull ?? nonOpPull,
    cancel ?? nonOpCancel,
    0,
    0);
}

// Thin re-exports of native controller operations (other internal modules call
// these by name).
function readableStreamDefaultControllerEnqueue(controller, chunk) {
  return controller.enqueue(chunk);
}

function readableStreamDefaultControllerClose(controller) {
  return controller.close();
}

function readableStreamDefaultControllerError(controller, error) {
  return controller.error(error);
}

// Cancels a stream via the public API. NOTE: callers may pass an unlocked
// stream. The native cancel() rejects on a locked stream, whereas the spec's
// internal ReadableStreamCancel ignores locks. None of the current internal
// callers (pipeTo, tee) pass a locked stream, so this maps directly.
// FIXME(flip): if a WPT/parallel test reveals an internal caller relies on
// cancelling a locked stream, this needs a lock-bypassing native helper.
function readableStreamCancel(stream, reason) {
  return stream.cancel(reason);
}

// ---- from(iterable) ----

function readableStreamFromIterable(iterable) {
  let stream;
  const iteratorGetter =
    iterable[SymbolAsyncIterator] ?? iterable[SymbolIterator];
  if (iteratorGetter == null || typeof iteratorGetter !== 'function') {
    throw new ERR_ARG_NOT_ITERABLE(iterable);
  }
  const iterator = ReflectApply(iteratorGetter, iterable, []);
  if (iterator === null ||
      (typeof iterator !== 'object' && typeof iterator !== 'function')) {
    throw new ERR_INVALID_STATE.TypeError(
      'The iterator method must return an object');
  }

  async function pullAlgorithm(controller) {
    const iterResult = await iterator.next();
    if (typeof iterResult !== 'object' || iterResult === null) {
      throw new ERR_INVALID_STATE.TypeError(
        'The promise returned by the iterator.next() method must fulfill ' +
        'with an object');
    }
    if (iterResult.done) {
      controller.close();
    } else {
      controller.enqueue(await iterResult.value);
    }
  }

  async function cancelAlgorithm(reason) {
    const returnMethod = iterator.return;
    if (returnMethod === undefined) {
      return;
    }
    const iterResult = await ReflectApply(returnMethod, iterator, [reason]);
    if (typeof iterResult !== 'object' || iterResult === null) {
      throw new ERR_INVALID_STATE.TypeError(
        'The promise returned by the iterator.return() method must fulfill ' +
        'with an object');
    }
  }

  stream = createReadableStream(
    nonOpStart,
    pullAlgorithm,
    cancelAlgorithm,
    0);

  return stream;
}

// ---- pipeTo ----

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

  // The error here can be undefined. The rejected arg tells us that the
  // promise must be rejected even when error is undefined.
  function finalize(rejected, error) {
    writer.releaseLock();
    reader.releaseLock();
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

  // Mirrors the C++ WritableState enum: 0 writable, 1 erroring, 2 closed,
  // 3 errored.
  function destState() {
    return writableStreamStateField(dest);
  }

  // A destination still accepts writes if it is writable with no close queued
  // or in flight (matches the spec's `state === "writable" &&
  // !writableStreamCloseQueuedOrInFlight`).
  function destAcceptsWrites() {
    return destState() === 0 && !writableStreamCloseQueuedOrInFlight(dest);
  }

  function shutdownWithAnAction(action, rejected, originalError) {
    if (shuttingDown) return;
    shuttingDown = true;
    if (destAcceptsWrites()) {
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
    if (destAcceptsWrites()) {
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
        () => writer.abort(error));
    }
    if (!preventCancel) {
      ArrayPrototypePush(
        actions,
        () => reader.cancel(error));
    }

    shutdownWithAnAction(
      () => SafePromiseAll(actions, (action) => action()),
      true,
      error);
  }

  // Wait for the source to become readable-or-terminal and react. The reader's
  // closed promise resolves on close and rejects on error.
  function watchErrored(closedPromise, action) {
    PromisePrototypeThen(closedPromise, undefined, action);
  }

  function watchClosed(closedPromise, action) {
    PromisePrototypeThen(closedPromise, action, () => {});
  }

  async function step() {
    if (shuttingDown) return true;

    // Honor backpressure by waiting until the writer is ready.
    await writer.ready;
    if (shuttingDown) return true;

    const result = await reader.read();
    // A chunk that has already been read must still be written, even if a
    // shutdown began while the read was pending (reading the final buffered
    // chunk is what triggers source close). Only a `done` result stops the loop.
    if (result.done) {
      return true;
    }

    // Per spec, pipeTo writes without awaiting completion here; the write
    // promise is tracked so a clean shutdown can wait on the last write.
    state.currentWrite = writer.write(result.value);
    markPromiseAsHandled(state.currentWrite);
    return false;
  }

  async function run() {
    // Run until step resolves as true.
    while (!await step());
  }

  if (signal !== undefined) {
    if (signal.aborted) {
      abortAlgorithm();
      return promise.promise;
    }
    addAbortListener ??=
      require('internal/events/abort_listener').addAbortListener;
    disposable = addAbortListener(signal, abortAlgorithm);
  }

  setPromiseHandled(run());

  watchErrored(reader.closed, (error) => {
    if (!preventAbort) {
      return shutdownWithAnAction(
        () => writer.abort(error),
        true,
        error);
    }
    shutdown(true, error);
  });

  watchErrored(writer.closed, (error) => {
    if (!preventCancel) {
      return shutdownWithAnAction(
        () => reader.cancel(error),
        true,
        error);
    }
    shutdown(true, error);
  });

  watchClosed(reader.closed, () => {
    if (!preventClose) {
      return shutdownWithAnAction(
        () => writer.close());
    }
    shutdown();
  });

  // Only a closed (or close-queued) destination yields the synthetic "closed"
  // error here; an errored destination is handled by the writer.closed
  // reaction above so the actual stored error propagates.
  if (writableStreamCloseQueuedOrInFlight(dest) || destState() === 2) {
    const error = new ERR_INVALID_STATE.TypeError(
      'Destination WritableStream is closed');
    if (!preventCancel) {
      shutdownWithAnAction(
        () => reader.cancel(error), true, error);
    } else {
      shutdown(true, error);
    }
  }

  return promise.promise;
}

// ---- tee ----

function readableStreamTee(stream, cloneForBranch2) {
  if (stream[kIsByteStream] === true) {
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
  let branch1Controller;
  let branch2Controller;
  const cancelPromise = PromiseWithResolvers();

  function pullAlgorithm() {
    if (reading) return PromiseResolve();
    reading = true;
    PromisePrototypeThen(
      reader.read(),
      (result) => {
        reading = false;
        if (result.done) {
          // The `process.nextTick()` is not part of the spec. This approach
          // was needed to avoid a race condition working with esm. Further
          // information, see: https://github.com/nodejs/node/issues/39758
          process.nextTick(() => {
            if (!canceled1)
              branch1Controller.close();
            if (!canceled2)
              branch2Controller.close();
            if (!canceled1 || !canceled2)
              cancelPromise.resolve();
          });
          return;
        }
        queueMicrotask(() => {
          const value1 = result.value;
          let value2 = result.value;
          if (!canceled2 && cloneForBranch2) {
            value2 = structuredClone(value2);
          }
          if (!canceled1) {
            branch1Controller.enqueue(value1);
          }
          if (!canceled2) {
            branch2Controller.enqueue(value2);
          }
        });
      },
      () => {
        reading = false;
      });
    return PromiseResolve();
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

  branch1 = new ReadableStream({
    start(c) { branch1Controller = c; },
    pull: pullAlgorithm,
    cancel: cancel1Algorithm,
  });
  branch2 = new ReadableStream({
    start(c) { branch2Controller = c; },
    pull: pullAlgorithm,
    cancel: cancel2Algorithm,
  });

  PromisePrototypeThen(
    reader.closed,
    undefined,
    (error) => {
      branch1Controller.error(error);
      branch2Controller.error(error);
      if (!canceled1 || !canceled2)
        cancelPromise.resolve();
    });

  return [branch1, branch2];
}

function readableByteStreamTee(stream) {
  assert(isReadableStream(stream));

  let reader = new ReadableStreamDefaultReader(stream);
  let readerIsByob = false;
  let reading = false;
  let readAgainForBranch1 = false;
  let readAgainForBranch2 = false;
  let canceled1 = false;
  let canceled2 = false;
  let reason1;
  let reason2;
  let branch1;
  let branch2;
  let branch1Controller;
  let branch2Controller;
  const cancelDeferred = PromiseWithResolvers();

  function forwardReaderError(thisReader) {
    PromisePrototypeThen(
      thisReader.closed,
      undefined,
      (error) => {
        if (thisReader !== reader) {
          return;
        }
        branch1Controller.error(error);
        branch2Controller.error(error);
        if (!canceled1 || !canceled2) {
          cancelDeferred.resolve();
        }
      },
    );
  }

  function pullWithDefaultReader() {
    if (readerIsByob) {
      reader.releaseLock();
      reader = new ReadableStreamDefaultReader(stream);
      readerIsByob = false;
      forwardReaderError(reader);
    }

    PromisePrototypeThen(
      reader.read(),
      (result) => {
        if (result.done) {
          reading = false;

          if (!canceled1) {
            branch1Controller.close();
          }
          if (!canceled2) {
            branch2Controller.close();
          }
          if (branch1Controller.byobRequest !== null) {
            branch1Controller.byobRequest.respond(0);
          }
          if (branch2Controller.byobRequest !== null) {
            branch2Controller.byobRequest.respond(0);
          }
          if (!canceled1 || !canceled2) {
            cancelDeferred.resolve();
          }
          return;
        }
        const chunk = result.value;
        queueMicrotask(() => {
          readAgainForBranch1 = false;
          readAgainForBranch2 = false;
          const chunk1 = chunk;
          let chunk2 = chunk;

          if (!canceled1 && !canceled2) {
            try {
              chunk2 = cloneAsUint8Array(chunk);
            } catch (error) {
              branch1Controller.error(error);
              branch2Controller.error(error);
              cancelDeferred.resolve(readableStreamCancel(stream, error));
              return;
            }
          }
          if (!canceled1) {
            branch1Controller.enqueue(chunk1);
          }
          if (!canceled2) {
            branch2Controller.enqueue(chunk2);
          }
          reading = false;

          if (readAgainForBranch1) {
            pull1Algorithm();
          } else if (readAgainForBranch2) {
            pull2Algorithm();
          }
        });
      },
      () => {
        reading = false;
      });
  }

  function pullWithBYOBReader(view, forBranch2) {
    if (!readerIsByob) {
      reader.releaseLock();
      reader = new ReadableStreamBYOBReader(stream);
      readerIsByob = true;
      forwardReaderError(reader);
    }

    const byobBranchController = forBranch2 === true ?
      branch2Controller : branch1Controller;
    const otherBranchController = forBranch2 === false ?
      branch2Controller : branch1Controller;
    PromisePrototypeThen(
      reader.read(view, { min: 1 }),
      (result) => {
        const chunk = result.done ? undefined : result.value;
        if (result.done) {
          reading = false;

          const byobCanceled = forBranch2 === true ? canceled2 : canceled1;
          const otherCanceled = forBranch2 === false ? canceled2 : canceled1;

          if (!byobCanceled) {
            byobBranchController.close();
          }
          if (!otherCanceled) {
            otherBranchController.close();
          }
          if (chunk !== undefined) {
            if (!byobCanceled) {
              byobBranchController.byobRequest.respondWithNewView(chunk);
            }
            if (
              !otherCanceled &&
              otherBranchController.byobRequest !== null
            ) {
              otherBranchController.byobRequest.respond(0);
            }
          }
          if (!byobCanceled || !otherCanceled) {
            cancelDeferred.resolve();
          }
          return;
        }
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
              byobBranchController.error(error);
              otherBranchController.error(error);
              cancelDeferred.resolve(readableStreamCancel(stream, error));
              return;
            }
            if (!byobCanceled) {
              byobBranchController.byobRequest.respondWithNewView(chunk);
            }

            otherBranchController.enqueue(clonedChunk);
          } else if (!byobCanceled) {
            byobBranchController.byobRequest.respondWithNewView(chunk);
          }
          reading = false;

          if (readAgainForBranch1) {
            pull1Algorithm();
          } else if (readAgainForBranch2) {
            pull2Algorithm();
          }
        });
      },
      () => {
        reading = false;
      });
  }

  function pull1Algorithm() {
    if (reading) {
      readAgainForBranch1 = true;
      return PromiseResolve();
    }
    reading = true;

    const byobRequest = branch1Controller.byobRequest;
    if (byobRequest === null) {
      pullWithDefaultReader();
    } else {
      pullWithBYOBReader(byobRequest.view, false);
    }
    return PromiseResolve();
  }

  function pull2Algorithm() {
    if (reading) {
      readAgainForBranch2 = true;
      return PromiseResolve();
    }
    reading = true;

    const byobRequest = branch2Controller.byobRequest;
    if (byobRequest === null) {
      pullWithDefaultReader();
    } else {
      pullWithBYOBReader(byobRequest.view, true);
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

  branch1 = new ReadableStream({
    type: 'bytes',
    start(c) { branch1Controller = c; },
    pull: pull1Algorithm,
    cancel: cancel1Algorithm,
  });
  branch2 = new ReadableStream({
    type: 'bytes',
    start(c) { branch2Controller = c; },
    pull: pull2Algorithm,
    cancel: cancel2Algorithm,
  });

  forwardReaderError(reader);

  return [branch1, branch2];
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
  isReadableStreamLocked,
  isReadableStreamDefaultReader,
  isReadableStreamBYOBReader,
  isReadableByteStreamController,
  isReadableStreamBYOBRequest,
  isReadableStreamDefaultController,

  readableStreamPipeTo,
  readableStreamTee,
  readableStreamCancel,
  readableStreamDefaultControllerEnqueue,
  readableStreamDefaultControllerClose,
  readableStreamDefaultControllerError,

  createReadableStream,
  createReadableByteStream,
};
