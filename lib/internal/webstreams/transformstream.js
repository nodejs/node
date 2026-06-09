'use strict';

const {
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
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
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  DOMException,
} = internalBinding('messaging');

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
  nonOpCancel,
  nonOpFlush,
} = require('internal/webstreams/util');

const {
  TransformStream: TransformStreamNative,
  TransformStreamDefaultController: TransformStreamDefaultControllerNative,
  createTransformStream: createTransformStreamNative,
} = internalBinding('webstreams');

// A TransformStream's readable/writable halves are created in C++ and share the
// native ReadableStream/WritableStream prototypes. The public decorations
// (getReader/getWriter/pipeTo/inspect/transfer/…) are grafted onto those
// prototypes when these modules load — but a transform-only program may never
// touch the ReadableStream/WritableStream globals, so require them eagerly to
// guarantee the grafts are installed. (readablestream pulls in writablestream.)
require('internal/webstreams/readablestream');

/**
 * @typedef {import('./queuingstrategies').QueuingStrategy} QueuingStrategy
 */

const isTransformStream = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(TransformStreamNative.prototype, value);
const isTransformStreamDefaultController = (value) =>
  value != null &&
  ObjectPrototypeIsPrototypeOf(TransformStreamDefaultControllerNative.prototype,
                              value);

const hiddenMethodProp = (value) => ({
  __proto__: null, configurable: true, enumerable: false, writable: true, value,
});
const ctorProp = (value) => ({
  __proto__: null, configurable: true, enumerable: false, writable: true, value,
});

// Default transform when the transformer provides none: identity enqueue.
async function defaultTransformAlgorithm(chunk, controller) {
  controller.enqueue(chunk);
}

/**
 * The public TransformStream interface; the returned object is the C++
 * TransformStream that orchestrates its readable + writable halves.
 */
function TransformStream(
  transformer = kEmptyObject,
  writableStrategy = kEmptyObject,
  readableStrategy = kEmptyObject) {
  if (new.target === undefined)
    throw new ERR_ILLEGAL_CONSTRUCTOR();
  validateObject(transformer, 'transformer', kValidateObjectAllowObjects);
  validateObject(writableStrategy, 'writableStrategy',
                 kValidateObjectAllowObjectsAndNull);
  validateObject(readableStrategy, 'readableStrategy',
                 kValidateObjectAllowObjectsAndNull);
  const readableType = transformer?.readableType;
  const writableType = transformer?.writableType;
  const start = transformer?.start;
  const transform = transformer?.transform;
  const flush = transformer?.flush;
  const cancel = transformer?.cancel;

  if (readableType !== undefined) {
    throw new ERR_INVALID_ARG_VALUE.RangeError(
      'transformer.readableType', readableType);
  }
  if (writableType !== undefined) {
    throw new ERR_INVALID_ARG_VALUE.RangeError(
      'transformer.writableType', writableType);
  }

  const { mode: readableSizeMode, size: readableSize } =
    extractSizeMode(readableStrategy?.size);
  const readableHighWaterMark =
    extractHighWaterMark(readableStrategy?.highWaterMark, 0);
  const { mode: writableSizeMode, size: writableSize } =
    extractSizeMode(writableStrategy?.size);
  const writableHighWaterMark =
    extractHighWaterMark(writableStrategy?.highWaterMark, 1);

  const startAlgorithm = start !== undefined ?
    (controller) => ReflectApply(start, transformer, [controller]) :
    () => undefined;
  const transformAlgorithm = transform !== undefined ?
    createPromiseCallback('transformer.transform', transform, transformer) :
    defaultTransformAlgorithm;
  const flushAlgorithm = flush !== undefined ?
    createPromiseCallback('transformer.flush', flush, transformer) :
    nonOpFlush;
  const cancelAlgorithm = cancel !== undefined ?
    createPromiseCallback('transformer.cancel', cancel, transformer) :
    nonOpCancel;

  const stream = createTransformStreamNative(
    startAlgorithm,
    transformAlgorithm,
    flushAlgorithm,
    cancelAlgorithm,
    writableHighWaterMark,
    writableSizeMode,
    writableSize,
    readableHighWaterMark,
    readableSizeMode,
    readableSize,
    new AbortController());

  markTransferMode(stream, false, true);
  // The readable/writable halves are created in C++; mark them transferable so
  // `transform.readable`/`transform.writable` can themselves be transferred.
  markTransferMode(stream.readable, false, true);
  markTransferMode(stream.writable, false, true);
  // Support subclassing (new.target may be a subclass of TransformStream).
  if (new.target !== TransformStream)
    ObjectSetPrototypeOf(stream, new.target.prototype);
  return stream;
}
ObjectDefineProperty(TransformStream, 'prototype', {
  __proto__: null,
  value: TransformStreamNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function transformStreamInspect(depth, options) {
  return customInspect(depth, options, 'TransformStream', {
    readable: this.readable,
    writable: this.writable,
  });
}

function transformStreamTransfer() {
  if (!isTransformStream(this))
    throw new ERR_INVALID_THIS('TransformStream');
  const readable = this.readable;
  const writable = this.writable;
  if (readable.locked) {
    throw new DOMException(
      'Cannot transfer a locked ReadableStream', 'DataCloneError');
  }
  if (writable.locked) {
    throw new DOMException(
      'Cannot transfer a locked WritableStream', 'DataCloneError');
  }
  return {
    data: { readable, writable },
    deserializeInfo:
      'internal/webstreams/transformstream:TransferredTransformStream',
  };
}

function transformStreamTransferList() {
  return [this.readable, this.writable];
}

// The native readable/writable accessors brand-check via a V8 signature, which
// throws a bare "Illegal invocation" TypeError. Node's public surface requires
// an ERR_INVALID_THIS-coded TypeError, so wrap them with JS getters that brand
// check first and then delegate to the captured native getter.
const nativeReadableGet =
  ObjectGetOwnPropertyDescriptor(TransformStreamNative.prototype, 'readable').get;
const nativeWritableGet =
  ObjectGetOwnPropertyDescriptor(TransformStreamNative.prototype, 'writable').get;
// Named (via the `get x()` syntax) so idlharness sees `get readable` /
// `get writable`; the descriptors are extracted without invoking the getters.
const namedGetters = {
  get readable() {
    if (!isTransformStream(this)) throw new ERR_INVALID_THIS('TransformStream');
    return ReflectApply(nativeReadableGet, this, []);
  },
  get writable() {
    if (!isTransformStream(this)) throw new ERR_INVALID_THIS('TransformStream');
    return ReflectApply(nativeWritableGet, this, []);
  },
};
const readableGetter =
  ObjectGetOwnPropertyDescriptor(namedGetters, 'readable').get;
const writableGetter =
  ObjectGetOwnPropertyDescriptor(namedGetters, 'writable').get;
const accessorProp = (get) => ({
  __proto__: null, configurable: true, enumerable: true, get,
});

ObjectDefineProperties(TransformStream.prototype, {
  constructor: ctorProp(TransformStream),
  readable: accessorProp(readableGetter),
  writable: accessorProp(writableGetter),
  [kInspect]: hiddenMethodProp(transformStreamInspect),
  [kTransfer]: hiddenMethodProp(transformStreamTransfer),
  [kTransferList]: hiddenMethodProp(transformStreamTransferList),
  [SymbolToStringTag]: getNonWritablePropertyDescriptor('TransformStream'),
});

// A transferred TransformStream is just a holder for its two (separately
// transferred) halves; it is not a live native TransformStream. Its readable/
// writable own accessors shadow the native prototype getters.
function TransferredTransformStream() {
  const obj = { __proto__: TransformStream.prototype };
  markTransferMode(obj, false, true);
  return obj;
}
ObjectDefineProperty(TransferredTransformStream, 'prototype', {
  __proto__: null,
  value: TransformStream.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function transformStreamDeserialize({ readable, writable }) {
  ObjectDefineProperties(this, {
    readable: { __proto__: null, configurable: true, enumerable: true,
                get() { return readable; } },
    writable: { __proto__: null, configurable: true, enumerable: true,
                get() { return writable; } },
  });
}
ObjectDefineProperty(TransformStream.prototype, kDeserialize,
                     hiddenMethodProp(transformStreamDeserialize));

/**
 * The public TransformStreamDefaultController interface. Not user-constructible.
 */
function TransformStreamDefaultController() {
  throw new ERR_ILLEGAL_CONSTRUCTOR();
}
ObjectDefineProperty(TransformStreamDefaultController, 'prototype', {
  __proto__: null,
  value: TransformStreamDefaultControllerNative.prototype,
  writable: false,
  enumerable: false,
  configurable: false,
});

function controllerInspect(depth, options) {
  return customInspect(depth, options, 'TransformStreamDefaultController', {});
}

ObjectDefineProperties(TransformStreamDefaultController.prototype, {
  constructor: ctorProp(TransformStreamDefaultController),
  [kInspect]: hiddenMethodProp(controllerInspect),
  [SymbolToStringTag]:
    getNonWritablePropertyDescriptor('TransformStreamDefaultController'),
});

module.exports = {
  TransformStream,
  TransformStreamDefaultController,
  TransferredTransformStream,

  // Exported Brand Checks
  isTransformStream,
  isTransformStreamDefaultController,
};
