'use strict';

const {
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectPrototypeIsPrototypeOf,
  ObjectSetPrototypeOf,
  ReflectApply,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  AbortController,
} = require('internal/abort_controller');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  DOMException,
} = internalBinding('messaging');

const {
  WritableStream: WritableStreamNative,
  WritableStreamDefaultWriter: WritableStreamDefaultWriterNative,
  WritableStreamDefaultController: WritableStreamDefaultControllerNative,
  createWritableStream: createWritableStreamNative,
  acquireWritableStreamDefaultWriter,
  writableStreamStateField,
  writableStreamClosedPromise,
  writableStreamControllerStream,
} = internalBinding('webstreams');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
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
  createPromiseCallback,
  customInspect,
  extractHighWaterMark,
  extractSizeMode,
  getNonWritablePropertyDescriptor,
  lazyTransfer,
  nonOpStart,
  nonOpWrite,
  nonOpCancel,
} = require('internal/webstreams/util');

const {
  kIsErrored,
  kIsWritable,
  kIsClosedPromise,
  kControllerErrorFunction,
} = require('internal/streams/utils');

// Mirrors the C++ WritableState enum (src/streams/writable_stream.h):
//   0 = writable, 1 = erroring, 2 = closed, 3 = errored
const kWritableStateNames = ['writable', 'erroring', 'closed', 'errored'];

// Per-stream transfer bookkeeping (cross-realm ports + pipe promise), kept as a
// private symbol on the native stream object rather than as JS wrapper state.
const kTransferState = Symbol('kTransferState');
// Deferred cross-realm sink for a deserialized stream; its port is bound once it
// becomes available in [kDeserialize].
const kCrossRealmSink = Symbol('kCrossRealmSink');

/**
 * @typedef {import('./queuingstrategies').QueuingStrategy} QueuingStrategy
 */

const isWritableStream = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(WritableStreamNative.prototype, value);
const isWritableStreamDefaultWriter = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(WritableStreamDefaultWriterNative.prototype,
                              value);
const isWritableStreamDefaultController = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(WritableStreamDefaultControllerNative.prototype,
                              value);

function isWritableStreamLocked(stream) {
  return stream.locked;
}

// Core creation path: build the native WritableStream from already-wrapped
// algorithm functions, then apply the JS-only decorations (transfer mode +
// node:stream interop error hook). The controller is captured from the start
// algorithm (run synchronously by the native setup) so the interop layer can
// error the stream via it.
function newWritableStream(startAlgorithm, writeAlgorithm, closeAlgorithm,
                           abortAlgorithm, highWaterMark, sizeMode, sizeFn) {
  let controller;
  const wrappedStart = (c) => {
    controller = c;
    return startAlgorithm(c);
  };
  const stream = createWritableStreamNative(
    wrappedStart,
    writeAlgorithm,
    closeAlgorithm,
    abortAlgorithm,
    highWaterMark,
    sizeMode,
    sizeFn,
    new AbortController());
  markTransferMode(stream, false, true);
  stream[kControllerErrorFunction] = (error) => controller.error(error);
  return stream;
}

// Builds the algorithm closures from an underlying sink dictionary.
function newWritableStreamFromSink(sink, highWaterMark, sizeMode, sizeFn) {
  const start = sink?.start;
  const write = sink?.write;
  const close = sink?.close;
  const abort = sink?.abort;
  const startAlgorithm = start !== undefined ?
    (controller) => ReflectApply(start, sink, [controller]) :
    nonOpStart;
  const writeAlgorithm = write !== undefined ?
    createPromiseCallback('sink.write', write, sink) :
    nonOpWrite;
  const closeAlgorithm = close !== undefined ?
    createPromiseCallback('sink.close', close, sink) :
    nonOpCancel;
  const abortAlgorithm = abort !== undefined ?
    createPromiseCallback('sink.abort', abort, sink) :
    nonOpCancel;
  return newWritableStream(startAlgorithm, writeAlgorithm, closeAlgorithm,
                           abortAlgorithm, highWaterMark, sizeMode, sizeFn);
}

/**
 * The public WritableStream interface. The returned object is the C++
 * WritableStream; all per-operation logic lives in C++.
 */
function WritableStream(sink = kEmptyObject, strategy = kEmptyObject) {
  if (new.target === undefined)
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  validateObject(sink, 'sink', kValidateObjectAllowObjects);
  validateObject(strategy, 'strategy', kValidateObjectAllowObjectsAndNull);
  const type = sink?.type;
  if (type !== undefined)
    throw new ERR_INVALID_ARG_VALUE.RangeError('type', type);

  // Order matters for WPT: strategy is consulted before the sink members.
  const { mode, size } = extractSizeMode(strategy?.size);
  const highWaterMark = extractHighWaterMark(strategy?.highWaterMark, 1);

  const stream = newWritableStreamFromSink(sink, highWaterMark, mode, size);
  // Support subclassing (new.target may be a subclass of WritableStream).
  if (new.target !== WritableStream)
    ObjectSetPrototypeOf(stream, new.target.prototype);
  return stream;
}
ObjectDefineProperty(WritableStream, 'prototype', {
  __proto__: null,
  value: WritableStreamNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function getWriter() {
  if (!isWritableStream(this))
    throw new ERR_INVALID_THIS('WritableStream');
  return new WritableStreamDefaultWriter(this);
}

function writableStreamInspect(depth, options) {
  return customInspect(depth, options, 'WritableStream', {
    locked: this.locked,
    state: kWritableStateNames[writableStreamStateField(this)],
  });
}

function writableStreamTransfer() {
  if (!isWritableStream(this))
    throw new ERR_INVALID_THIS('WritableStream');
  if (this.locked) {
    this[kTransferState]?.port1?.close();
    this[kTransferState] = undefined;
    throw new DOMException(
      'Cannot transfer a locked WritableStream',
      'DataCloneError');
  }

  const transferState = this[kTransferState];
  const {
    readable,
    promise,
  } = lazyTransfer().newCrossRealmReadableStream(this, transferState.port1);

  transferState.readable = readable;
  transferState.promise = promise;

  return {
    data: { port: transferState.port2 },
    deserializeInfo:
      'internal/webstreams/writablestream:TransferredWritableStream',
  };
}

function writableStreamTransferList() {
  const { port1, port2 } = new MessageChannel();
  this[kTransferState] = {
    __proto__: null,
    port1,
    port2,
    readable: undefined,
    promise: undefined,
  };
  return [port2];
}

function writableStreamDeserialize({ port }) {
  // `this` is the native WritableStream from TransferredWritableStream(); bind
  // the now-available port into its deferred cross-realm sink.
  const transfer = lazyTransfer();
  this[kCrossRealmSink]?.[transfer.kBindPort](port);
}

// Native prototype members (locked/abort/close, and the writer/controller
// members) are already enumerable+configurable, so we only graft the JS-only
// additions here. `constructor` is repointed at the public interface object.
const methodProp = (value) => ({
  __proto__: null, configurable: true, enumerable: true, writable: true, value,
});
const hiddenMethodProp = (value) => ({
  __proto__: null, configurable: true, enumerable: false, writable: true, value,
});
const ctorProp = (value) => ({
  __proto__: null, configurable: true, enumerable: false, writable: true, value,
});

ObjectDefineProperties(WritableStream.prototype, {
  constructor: ctorProp(WritableStream),
  getWriter: methodProp(getWriter),
  [kInspect]: hiddenMethodProp(writableStreamInspect),
  [kTransfer]: hiddenMethodProp(writableStreamTransfer),
  [kTransferList]: hiddenMethodProp(writableStreamTransferList),
  [kDeserialize]: hiddenMethodProp(writableStreamDeserialize),
  [kIsErrored]: { __proto__: null, configurable: true, enumerable: false,
                  get() { return writableStreamStateField(this) === 3; } },
  [kIsWritable]: { __proto__: null, configurable: true, enumerable: false,
                   get() { return writableStreamStateField(this) === 0; } },
  [kIsClosedPromise]: { __proto__: null, configurable: true, enumerable: false,
                        get() {
                          return { promise: writableStreamClosedPromise(this) };
                        } },
  [SymbolToStringTag]: getNonWritablePropertyDescriptor('WritableStream'),
});

// Produces a native WritableStream wired to a deferred cross-realm sink; the
// deserializeInfo target for a transferred WritableStream.
function TransferredWritableStream() {
  const transfer = lazyTransfer();
  const sink = new transfer.CrossRealmTransformWritableSink(undefined, true);
  const stream = newWritableStreamFromSink(sink, 1, 0, undefined);
  stream[kCrossRealmSink] = sink;
  return stream;
}
ObjectDefineProperty(TransferredWritableStream, 'prototype', {
  __proto__: null,
  value: WritableStream.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

/**
 * The public WritableStreamDefaultWriter interface. Acquiring a writer builds
 * the C++ writer (which locks the stream).
 */
function WritableStreamDefaultWriter(stream) {
  if (new.target === undefined)
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  if (!isWritableStream(stream))
    throw new ERR_INVALID_ARG_TYPE('stream', 'WritableStream', stream);
  return acquireWritableStreamDefaultWriter(stream);
}
ObjectDefineProperty(WritableStreamDefaultWriter, 'prototype', {
  __proto__: null,
  value: WritableStreamDefaultWriterNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function writerInspect(depth, options) {
  return customInspect(depth, options, 'WritableStreamDefaultWriter', {
    closed: this.closed,
    ready: this.ready,
  });
}

ObjectDefineProperties(WritableStreamDefaultWriter.prototype, {
  constructor: ctorProp(WritableStreamDefaultWriter),
  [kInspect]: hiddenMethodProp(writerInspect),
  [SymbolToStringTag]:
    getNonWritablePropertyDescriptor('WritableStreamDefaultWriter'),
});

/**
 * The public WritableStreamDefaultController interface. Not user-constructible.
 */
function WritableStreamDefaultController() {
  throw new ERR_ILLEGAL_CONSTRUCTOR();
}
ObjectDefineProperty(WritableStreamDefaultController, 'prototype', {
  __proto__: null,
  value: WritableStreamDefaultControllerNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function controllerInspect(depth, options) {
  return customInspect(depth, options, 'WritableStreamDefaultController', {
    stream: writableStreamControllerStream(this),
  });
}

ObjectDefineProperties(WritableStreamDefaultController.prototype, {
  constructor: ctorProp(WritableStreamDefaultController),
  [kInspect]: hiddenMethodProp(controllerInspect),
  [SymbolToStringTag]:
    getNonWritablePropertyDescriptor('WritableStreamDefaultController'),
});

// Internal factory mirroring the old createWritableStream signature, used by
// internal callers that build a stream from raw algorithms.
function createWritableStream(start, write, close, abort,
                              highWaterMark = 1, size = () => 1) {
  const { mode, size: sizeFn } = extractSizeMode(size);
  return newWritableStream(
    start ?? nonOpStart,
    write ?? nonOpWrite,
    close ?? nonOpCancel,
    abort ?? nonOpCancel,
    highWaterMark,
    mode,
    sizeFn);
}

// transfer.js calls this to error a stream's controller if it is still
// writable. The native controller.error() already no-ops once the stream is no
// longer writable, so this maps directly.
function writableStreamDefaultControllerErrorIfNeeded(controller, error) {
  controller.error(error);
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
  writableStreamDefaultControllerErrorIfNeeded,
  createWritableStream,
};
