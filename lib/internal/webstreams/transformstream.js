'use strict';

const {
  FunctionPrototypeCall,
  ObjectDefineProperties,
  ObjectSetPrototypeOf,
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
  createPromiseCallback1Param,
  createRawCallback2Params,
  customInspect,
  extractHighWaterMark,
  extractSizeAlgorithm,
  getNonWritablePropertyDescriptor,
  isBrandCheck,
  kParkedAlgorithmResult,
  kResolvedPromise,
  kState,
  kType,
  nonOpCancel,
  nonOpFlush,
} = require('internal/webstreams/util');

const {
  createReadableStream,
  readableStreamDefaultControllerCanCloseOrEnqueue,
  readableStreamDefaultControllerClose,
  readableStreamDefaultControllerEnqueue,
  readableStreamDefaultControllerError,
  readableStreamDefaultControllerGetDesiredSize,
  readableStreamDefaultControllerHasBackpressure,
} = require('internal/webstreams/readablestream');

const {
  createWritableStream,
  writableStreamDefaultControllerErrorIfNeeded,
} = require('internal/webstreams/writablestream');

const assert = require('internal/assert');

const kSkipThrow = Symbol('kSkipThrow');

/**
 * @typedef {import('./queuingstrategies').QueuingStrategy
 * } QueuingStrategy
 * @typedef {import('./queuingstrategies').QueuingStrategySize
 * } QueuingStrategySize
 */

/**
 * @callback TransformerStartCallback
 * @param {TransformStreamDefaultController} controller
 */

/**
 * @callback TransformerFlushCallback
 * @param {TransformStreamDefaultController} controller
 * @returns {Promise<void>}
 */

/**
 * @callback TransformerTransformCallback
 * @param {any} chunk
 * @param {TransformStreamDefaultController} controller
 * @returns {Promise<void>}
 */

/**
 * @typedef {{
 *  start? : TransformerStartCallback,
 *  transform? : TransformerTransformCallback,
 *  flush? : TransformerFlushCallback,
 *  readableType? : any,
 *  writableType? : any,
 * }} Transformer
 */

class TransformStream {
  [kType] = 'TransformStream';

  /**
   * @param {Transformer} [transformer]
   * @param {QueuingStrategy} [writableStrategy]
   * @param {QueuingStrategy} [readableStrategy]
   */
  constructor(
    transformer = kEmptyObject,
    writableStrategy = kEmptyObject,
    readableStrategy = kEmptyObject) {
    markTransferMode(this, false, true);
    if (transformer !== kEmptyObject) {
      validateObject(transformer, 'transformer', kValidateObjectAllowObjects);
    }
    if (writableStrategy !== kEmptyObject) {
      validateObject(writableStrategy, 'writableStrategy', kValidateObjectAllowObjectsAndNull);
    }
    if (readableStrategy !== kEmptyObject) {
      validateObject(readableStrategy, 'readableStrategy', kValidateObjectAllowObjectsAndNull);
    }
    const readableType = transformer?.readableType;
    const writableType = transformer?.writableType;
    const start = transformer?.start;

    if (readableType !== undefined) {
      throw new ERR_INVALID_ARG_VALUE.RangeError(
        'transformer.readableType',
        readableType);
    }
    if (writableType !== undefined) {
      throw new ERR_INVALID_ARG_VALUE.RangeError(
        'transformer.writableType',
        writableType);
    }

    const readableHighWaterMark = readableStrategy?.highWaterMark;
    const readableSize = readableStrategy?.size;

    const writableHighWaterMark = writableStrategy?.highWaterMark;
    const writableSize = writableStrategy?.size;

    const actualReadableHighWaterMark =
      extractHighWaterMark(readableHighWaterMark, 0);
    const actualReadableSize = extractSizeAlgorithm(readableSize);

    const actualWritableHighWaterMark =
      extractHighWaterMark(writableHighWaterMark, 1);
    const actualWritableSize = extractSizeAlgorithm(writableSize);

    const startPromise = PromiseWithResolvers();

    initializeTransformStream(
      this,
      startPromise,
      actualWritableHighWaterMark,
      actualWritableSize,
      actualReadableHighWaterMark,
      actualReadableSize);

    setupTransformStreamDefaultControllerFromTransformer(this, transformer);

    if (start !== undefined) {
      startPromise.resolve(
        FunctionPrototypeCall(
          start,
          transformer,
          this[kState].controller));
    } else {
      startPromise.resolve();
    }
  }

  /**
   * @readonly
   * @type {ReadableStream}
   */
  get readable() {
    if (!isTransformStream(this))
      throw new ERR_INVALID_THIS('TransformStream');
    return this[kState].readable;
  }

  /**
   * @readonly
   * @type {WritableStream}
   */
  get writable() {
    if (!isTransformStream(this))
      throw new ERR_INVALID_THIS('TransformStream');
    return this[kState].writable;
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      readable: this.readable,
      writable: this.writable,
      backpressure: this[kState].backpressure,
    });
  }

  [kTransfer]() {
    if (!isTransformStream(this))
      throw new ERR_INVALID_THIS('TransformStream');
    const {
      readable,
      writable,
    } = this[kState];
    if (readable.locked) {
      throw new DOMException(
        'Cannot transfer a locked ReadableStream',
        'DataCloneError');
    }
    if (writable.locked) {
      throw new DOMException(
        'Cannot transfer a locked WritableStream',
        'DataCloneError');
    }
    return {
      data: {
        readable,
        writable,
      },
      deserializeInfo:
        'internal/webstreams/transformstream:TransferredTransformStream',
    };
  }

  [kTransferList]() {
    return [ this[kState].readable, this[kState].writable ];
  }

  [kDeserialize]({ readable, writable }) {
    this[kState].readable = readable;
    this[kState].writable = writable;
  }
}

ObjectDefineProperties(TransformStream.prototype, {
  readable: kEnumerableProperty,
  writable: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(TransformStream.name),
});

function InternalTransferredTransformStream() {
  ObjectSetPrototypeOf(this, TransformStream.prototype);
  markTransferMode(this, false, true);
  this[kType] = 'TransformStream';
  this[kState] = {
    __proto__: null,
    readable: undefined,
    writable: undefined,
    backpressure: undefined,
    pullPending: false,
    pendingWrite: undefined,
    pendingWriteChunk: undefined,
    writeContinuation: undefined,
    controller: undefined,
  };
}

ObjectSetPrototypeOf(InternalTransferredTransformStream.prototype, TransformStream.prototype);
ObjectSetPrototypeOf(InternalTransferredTransformStream, TransformStream);

function TransferredTransformStream() {
  const stream = new InternalTransferredTransformStream();

  stream.constructor = TransformStream;

  return stream;
}

TransferredTransformStream.prototype[kDeserialize] = () => {};

class TransformStreamDefaultController {
  [kType] = 'TransformStreamDefaultController';

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
    if (!isTransformStreamDefaultController(this))
      throw new ERR_INVALID_THIS('TransformStreamDefaultController');
    const {
      stream,
    } = this[kState];
    const {
      readable,
    } = stream[kState];
    const {
      controller: readableController,
    } = readable[kState];
    return readableStreamDefaultControllerGetDesiredSize(readableController);
  }

  /**
   * @param {any} [chunk]
   */
  enqueue(chunk = undefined) {
    if (!isTransformStreamDefaultController(this))
      throw new ERR_INVALID_THIS('TransformStreamDefaultController');
    transformStreamDefaultControllerEnqueue(this, chunk);
  }

  /**
   * @param {any} [reason]
   */
  error(reason = undefined) {
    if (!isTransformStreamDefaultController(this))
      throw new ERR_INVALID_THIS('TransformStreamDefaultController');
    transformStreamDefaultControllerError(this, reason);
  }

  terminate() {
    if (!isTransformStreamDefaultController(this))
      throw new ERR_INVALID_THIS('TransformStreamDefaultController');
    transformStreamDefaultControllerTerminate(this);
  }

  [kInspect](depth, options) {
    return customInspect(depth, options, this[kType], {
      stream: this[kState].stream,
    });
  }
}

ObjectDefineProperties(TransformStreamDefaultController.prototype, {
  desiredSize: kEnumerableProperty,
  enqueue: kEnumerableProperty,
  error: kEnumerableProperty,
  terminate: kEnumerableProperty,
  [SymbolToStringTag]: getNonWritablePropertyDescriptor(TransformStreamDefaultController.name),
});

const isTransformStream =
  isBrandCheck('TransformStream');
const isTransformStreamDefaultController =
  isBrandCheck('TransformStreamDefaultController');

// Raw callback (see createRawCallback*): invoked inside the try/catch of
// transformStreamDefaultControllerPerformTransform.
function defaultTransformAlgorithm(chunk, controller) {
  transformStreamDefaultControllerEnqueue(controller, chunk);
}

function initializeTransformStream(
  stream,
  startPromise,
  writableHighWaterMark,
  writableSizeAlgorithm,
  readableHighWaterMark,
  readableSizeAlgorithm) {

  const startAlgorithm = () => startPromise.promise;

  const writable = createWritableStream(
    startAlgorithm,
    (chunk) => transformStreamDefaultSinkWriteAlgorithm(stream, chunk),
    () => transformStreamDefaultSinkCloseAlgorithm(stream),
    (reason) => transformStreamDefaultSinkAbortAlgorithm(stream, reason),
    writableHighWaterMark,
    writableSizeAlgorithm,
  );

  const readable = createReadableStream(
    startAlgorithm,
    () => transformStreamDefaultSourcePullAlgorithm(stream),
    (reason) => transformStreamDefaultSourceCancelAlgorithm(stream, reason),
    readableHighWaterMark,
    readableSizeAlgorithm,
  );

  stream[kState] = {
    __proto__: null,
    readable,
    writable,
    controller: undefined,
    backpressure: undefined,
    // Continuation slots replacing the spec's
    // [[backpressureChangePromise]]; see transformStreamSetBackpressure.
    pullPending: false,
    pendingWrite: undefined,
    pendingWriteChunk: undefined,
    writeContinuation: undefined,
  };

  transformStreamSetBackpressure(stream, true);
}

function transformStreamError(stream, error) {
  const {
    readable,
  } = stream[kState];
  const {
    controller,
  } = readable[kState];
  readableStreamDefaultControllerError(controller, error);
  transformStreamErrorWritableAndUnblockWrite(stream, error);
}

function transformStreamErrorWritableAndUnblockWrite(stream, error) {
  const {
    controller,
    writable,
  } = stream[kState];
  transformStreamDefaultControllerClearAlgorithms(controller);
  writableStreamDefaultControllerErrorIfNeeded(
    writable[kState].controller,
    error);
  transformStreamUnblockWrite(stream);
}

function transformStreamUnblockWrite(stream) {
  if (stream[kState].backpressure)
    transformStreamSetBackpressure(stream, false);
}

// The spec's [[backpressureChangePromise]] is only ever observed by the
// source pull algorithm (settles when backpressure next becomes true) and
// by a sink write arriving while backpressure is set (settles when
// backpressure next becomes false). Both observers are internal, so the
// promise record is replaced by continuation slots: a parked pull is
// completed by delivering the readable controller's pull-fulfilled step,
// and a parked write by the cached write continuation (see
// transformStreamDefaultSinkWriteAlgorithm). Each is enqueued on the
// shared resolved promise at the exact microtask position the old
// record's reaction would have had.
function transformStreamSetBackpressure(stream, backpressure) {
  const state = stream[kState];
  assert(state.backpressure !== backpressure);
  state.backpressure = backpressure;
  if (backpressure) {
    if (state.pullPending) {
      state.pullPending = false;
      // The pull-fulfilled step exists: a pull parked it (see
      // transformStreamDefaultSourcePullAlgorithm), and the readable
      // controller creates it before invoking the pull algorithm.
      PromisePrototypeThen(
        kResolvedPromise,
        state.readable[kState].controller[kState].pullFulfilled);
    }
  } else if (state.pendingWrite !== undefined) {
    PromisePrototypeThen(kResolvedPromise, state.writeContinuation);
  }
}

function setupTransformStreamDefaultController(
  stream,
  controller,
  transformAlgorithm,
  flushAlgorithm,
  cancelAlgorithm) {
  assert(isTransformStream(stream));
  assert(stream[kState].controller === undefined);
  controller[kState] = {
    __proto__: null,
    stream,
    transformAlgorithm,
    flushAlgorithm,
    cancelAlgorithm,
    performTransformRejected: undefined,
  };
  stream[kState].controller = controller;
}

function setupTransformStreamDefaultControllerFromTransformer(
  stream,
  transformer) {
  const controller = new TransformStreamDefaultController(kSkipThrow);
  const transform = transformer?.transform;
  const flush = transformer?.flush;
  const cancel = transformer?.cancel;
  const transformAlgorithm = transform ?
    createRawCallback2Params('transformer.transform', transform, transformer) :
    defaultTransformAlgorithm;
  const flushAlgorithm = flush ?
    createPromiseCallback1Param('transformer.flush', flush, transformer) :
    nonOpFlush;
  const cancelAlgorithm = cancel ?
    createPromiseCallback1Param('transformer.cancel', cancel, transformer) :
    nonOpCancel;

  setupTransformStreamDefaultController(
    stream,
    controller,
    transformAlgorithm,
    flushAlgorithm,
    cancelAlgorithm);
}

function transformStreamDefaultControllerClearAlgorithms(controller) {
  controller[kState].transformAlgorithm = undefined;
  controller[kState].flushAlgorithm = undefined;
  controller[kState].cancelAlgorithm = undefined;
}

function transformStreamDefaultControllerEnqueue(controller, chunk) {
  const {
    stream,
  } = controller[kState];
  const {
    readable,
  } = stream[kState];
  const {
    controller: readableController,
  } = readable[kState];
  if (!readableStreamDefaultControllerCanCloseOrEnqueue(readableController))
    throw new ERR_INVALID_STATE.TypeError('Unable to enqueue');
  try {
    readableStreamDefaultControllerEnqueue(readableController, chunk);
  } catch (error) {
    transformStreamErrorWritableAndUnblockWrite(stream, error);
    throw readable[kState].storedError;
  }
  const backpressure =
    readableStreamDefaultControllerHasBackpressure(readableController);
  if (backpressure !== stream[kState].backpressure) {
    assert(backpressure);
    transformStreamSetBackpressure(stream, true);
  }
}

function transformStreamDefaultControllerError(controller, error) {
  transformStreamError(controller[kState].stream, error);
}

// Mirrors the reference implementation's
// `promiseCall(transformAlgorithm, ...).then(undefined, rejectionSteps)`:
// the returned promise settles one microtask after the (coerced) result
// does, and a rejection errors the transform stream before propagating.
// The raw transform callback plus the shared resolved promise for
// non-thenable results replace the previous async wrapper's two implicit
// promises per chunk.
function transformStreamDefaultControllerPerformTransform(controller, chunk) {
  const controllerState = controller[kState];
  const transformAlgorithm = controllerState.transformAlgorithm;
  if (transformAlgorithm === undefined) {
    // Algorithms were cleared by a concurrent cancel/abort/close.
    return kResolvedPromise;
  }
  let result;
  try {
    result = transformAlgorithm(chunk, controller);
  } catch (error) {
    result = PromiseReject(error);
  }
  if (result === null ||
      (typeof result !== 'object' && typeof result !== 'function')) {
    result = kResolvedPromise;
  } else {
    result = PromiseResolve(result);
  }
  controllerState.performTransformRejected ??= (error) => {
    transformStreamError(controller[kState].stream, error);
    throw error;
  };
  return PromisePrototypeThen(
    result,
    undefined,
    controllerState.performTransformRejected);
}

function transformStreamDefaultControllerTerminate(controller) {
  const {
    stream,
  } = controller[kState];
  const {
    readable,
  } = stream[kState];
  assert(readable !== undefined);
  const {
    controller: readableController,
  } = readable[kState];
  readableStreamDefaultControllerClose(readableController);
  transformStreamErrorWritableAndUnblockWrite(
    stream,
    new ERR_INVALID_STATE.TypeError('TransformStream has been terminated'));
}

function transformStreamDefaultSinkWriteAlgorithm(stream, chunk) {
  const state = stream[kState];
  const {
    writable,
    controller,
  } = state;
  assert(writable[kState].state === 'writable');
  if (state.backpressure) {
    // Park the chunk and one promise record; the backpressure -> false
    // flip delivers the cached continuation (see
    // transformStreamSetBackpressure) at the same microtask position as
    // the old [[backpressureChangePromise]] reaction. The continuation
    // resolves the sink promise with the perform-transform promise, so
    // adoption reproduces the old derived-chain settle depth exactly.
    // The writable dispatches a single write at a time, so one pending
    // slot suffices.
    assert(state.pendingWrite === undefined);
    const pendingWrite = PromiseWithResolvers();
    state.pendingWrite = pendingWrite;
    state.pendingWriteChunk = chunk;
    state.writeContinuation ??= () => {
      const pending = state.pendingWrite;
      const pendingChunk = state.pendingWriteChunk;
      state.pendingWrite = undefined;
      state.pendingWriteChunk = undefined;
      const writableState = state.writable[kState];
      if (writableState.state === 'erroring') {
        pending.reject(writableState.storedError);
        return;
      }
      assert(writableState.state === 'writable');
      pending.resolve(
        transformStreamDefaultControllerPerformTransform(
          controller,
          pendingChunk));
    };
    return pendingWrite.promise;
  }
  return transformStreamDefaultControllerPerformTransform(controller, chunk);
}

async function transformStreamDefaultSinkAbortAlgorithm(stream, reason) {
  const {
    controller,
    readable,
  } = stream[kState];

  if (controller[kState].finishPromise !== undefined) {
    return controller[kState].finishPromise;
  }

  const { promise, resolve, reject } = PromiseWithResolvers();
  controller[kState].finishPromise = promise;
  const cancelPromise = controller[kState].cancelAlgorithm(reason);
  transformStreamDefaultControllerClearAlgorithms(controller);

  PromisePrototypeThen(
    cancelPromise,
    () => {
      if (readable[kState].state === 'errored')
        reject(readable[kState].storedError);
      else {
        readableStreamDefaultControllerError(readable[kState].controller, reason);
        resolve();
      }
    },
    (error) => {
      readableStreamDefaultControllerError(readable[kState].controller, error);
      reject(error);
    },
  );

  return controller[kState].finishPromise;
}

function transformStreamDefaultSinkCloseAlgorithm(stream) {
  const {
    readable,
    controller,
  } = stream[kState];

  if (controller[kState].finishPromise !== undefined) {
    return controller[kState].finishPromise;
  }
  const { promise, resolve, reject } = PromiseWithResolvers();
  controller[kState].finishPromise = promise;
  const flushPromise = controller[kState].flushAlgorithm(controller);
  transformStreamDefaultControllerClearAlgorithms(controller);
  PromisePrototypeThen(
    flushPromise,
    () => {
      if (readable[kState].state === 'errored')
        reject(readable[kState].storedError);
      else {
        readableStreamDefaultControllerClose(readable[kState].controller);
        resolve();
      }
    },
    (error) => {
      readableStreamDefaultControllerError(readable[kState].controller, error);
      reject(error);
    });
  return controller[kState].finishPromise;
}

function transformStreamDefaultSourcePullAlgorithm(stream) {
  const state = stream[kState];
  assert(state.backpressure);
  transformStreamSetBackpressure(stream, false);
  // Park the pull: the next backpressure -> true flip delivers the
  // pull-fulfilled step (see transformStreamSetBackpressure). The old
  // [[backpressureChangePromise]] this replaces was only ever resolved,
  // so the parked pull needs no rejection delivery.
  state.pullPending = true;
  return kParkedAlgorithmResult;
}

function transformStreamDefaultSourceCancelAlgorithm(stream, reason) {
  const {
    controller,
    writable,
  } = stream[kState];

  if (controller[kState].finishPromise !== undefined) {
    return controller[kState].finishPromise;
  }

  const { promise, resolve, reject } = PromiseWithResolvers();
  controller[kState].finishPromise = promise;
  const cancelPromise = controller[kState].cancelAlgorithm(reason);
  transformStreamDefaultControllerClearAlgorithms(controller);

  PromisePrototypeThen(
    cancelPromise,
    () => {
      if (writable[kState].state === 'errored')
        reject(writable[kState].storedError);
      else {
        writableStreamDefaultControllerErrorIfNeeded(
          writable[kState].controller,
          reason);
        transformStreamUnblockWrite(stream);
        resolve();
      }
    },
    (error) => {
      writableStreamDefaultControllerErrorIfNeeded(
        writable[kState].controller,
        error);
      transformStreamUnblockWrite(stream);
      reject(error);
    },
  );

  return controller[kState].finishPromise;
}

module.exports = {
  TransformStream,
  TransformStreamDefaultController,
  TransferredTransformStream,

  // Exported Brand Checks
  isTransformStream,
  isTransformStreamDefaultController,
};
