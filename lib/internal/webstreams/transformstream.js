'use strict';

const {
  FunctionPrototypeBind,
  FunctionPrototypeCall,
  ObjectDefineProperties,
  PromisePrototypeCatch,
  PromisePrototypeThen,
  PromiseResolve,
  ReflectConstruct,
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
  createDeferredPromise,
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');

const {
  kDeserialize,
  kTransfer,
  kTransferList,
  makeTransferable,
} = require('internal/worker/js_transferable');

const {
  customInspect,
  ensureIsPromise,
  extractHighWaterMark,
  extractSizeAlgorithm,
  isBrandCheck,
  nonOpFlush,
  kType,
  kState,
} = require('internal/webstreams/util');

const {
  ReadableStream,
  readableStreamDefaultControllerCanCloseOrEnqueue,
  readableStreamDefaultControllerClose,
  readableStreamDefaultControllerEnqueue,
  readableStreamDefaultControllerError,
  readableStreamDefaultControllerGetDesiredSize,
  readableStreamDefaultControllerHasBackpressure,
} = require('internal/webstreams/readablestream');

const {
  WritableStream,
  writableStreamDefaultControllerErrorIfNeeded,
} = require('internal/webstreams/writablestream');

const assert = require('internal/assert');

/**
 * @typedef {import('./queuingstrategies').QueuingStrategy
 * } QueuingStrategy
 * @typedef {import('./queuingstrategies').QueuingStrategySize
 * } QueuingStrategySize
 */

/**
 * @callback TransformerStartCallback
 * @param {TransformStreamDefaultController} controller;
 */

/**
 * @callback TransformerFlushCallback
 * @param {TransformStreamDefaultController} controller;
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

  get [SymbolToStringTag]() { return this[kType]; }

  /**
   * @param {Transformer} [transformer]
   * @param {QueuingStrategy} [writableStrategy]
   * @param {QueuingStrategy} [readableStrategy]
   */
  constructor(
    transformer = null,
    writableStrategy = {},
    readableStrategy = {}) {
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

    const startPromise = createDeferredPromise();

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

    // eslint-disable-next-line no-constructor-return
    return makeTransferable(this);
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
        'internal/webstreams/transformstream:TransferredTransformStream'
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
});

function TransferredTransformStream() {
  return makeTransferable(ReflectConstruct(
    function() {
      this[kType] = 'TransformStream';
      this[kState] = {
        readable: undefined,
        writable: undefined,
        backpressure: undefined,
        backpressureChange: {
          promise: undefined,
          resolve: undefined,
          reject: undefined,
        },
        controller: undefined,
      };
    },
    [], TransformStream));
}
TransferredTransformStream.prototype[kDeserialize] = () => {};

class TransformStreamDefaultController {
  [kType] = 'TransformStreamDefaultController';

  get [SymbolToStringTag]() { return this[kType]; }

  constructor() {
    throw new ERR_ILLEGAL_CONSTRUCTOR();
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
   * @param {any} chunk
   */
  enqueue(chunk = undefined) {
    if (!isTransformStreamDefaultController(this))
      throw new ERR_INVALID_THIS('TransformStreamDefaultController');
    transformStreamDefaultControllerEnqueue(this, chunk);
  }

  /**
   * @param {any} reason
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
});

function createTransformStreamDefaultController() {
  return ReflectConstruct(
    function() {
      this[kType] = 'TransformStreamDefaultController';
    },
    [],
    TransformStreamDefaultController);
}

const isTransformStream =
  isBrandCheck('TransformStream');
const isTransformStreamDefaultController =
  isBrandCheck('TransformStreamDefaultController');

async function defaultTransformAlgorithm(chunk, controller) {
  transformStreamDefaultControllerEnqueue(controller, chunk);
}

function initializeTransformStream(
  stream,
  startPromise,
  writableHighWaterMark,
  writableSizeAlgorithm,
  readableHighWaterMark,
  readableSizeAlgorithm) {

  const writable = new WritableStream({
    start() { return startPromise.promise; },
    write(chunk) {
      return transformStreamDefaultSinkWriteAlgorithm(stream, chunk);
    },
    abort(reason) {
      return transformStreamDefaultSinkAbortAlgorithm(stream, reason);
    },
    close() {
      return transformStreamDefaultSinkCloseAlgorithm(stream);
    },
  }, {
    highWaterMark: writableHighWaterMark,
    size: writableSizeAlgorithm,
  });

  const readable = new ReadableStream({
    start() { return startPromise.promise; },
    pull() {
      return transformStreamDefaultSourcePullAlgorithm(stream);
    },
    cancel(reason) {
      transformStreamErrorWritableAndUnblockWrite(stream, reason);
      return PromiseResolve();
    },
  }, {
    highWaterMark: readableHighWaterMark,
    size: readableSizeAlgorithm,
  });

  stream[kState] = {
    readable,
    writable,
    controller: undefined,
    backpressure: undefined,
    backpressureChange: {
      promise: undefined,
      resolve: undefined,
      reject: undefined,
    }
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
  if (stream[kState].backpressure)
    transformStreamSetBackpressure(stream, false);
}

function transformStreamSetBackpressure(stream, backpressure) {
  assert(stream[kState].backpressure !== backpressure);
  if (stream[kState].backpressureChange.promise !== undefined)
    stream[kState].backpressureChange.resolve?.();
  stream[kState].backpressureChange = createDeferredPromise();
  stream[kState].backpressure = backpressure;
}

function setupTransformStreamDefaultController(
  stream,
  controller,
  transformAlgorithm,
  flushAlgorithm) {
  assert(isTransformStream(stream));
  assert(stream[kState].controller === undefined);
  controller[kState] = {
    stream,
    transformAlgorithm,
    flushAlgorithm,
  };
  stream[kState].controller = controller;
}

function setupTransformStreamDefaultControllerFromTransformer(
  stream,
  transformer) {
  const controller = createTransformStreamDefaultController();
  const transform = transformer?.transform || defaultTransformAlgorithm;
  const flush = transformer?.flush || nonOpFlush;
  const transformAlgorithm =
    FunctionPrototypeBind(transform, transformer);
  const flushAlgorithm =
    FunctionPrototypeBind(flush, transformer);

  setupTransformStreamDefaultController(
    stream,
    controller,
    transformAlgorithm,
    flushAlgorithm);
}

function transformStreamDefaultControllerClearAlgorithms(controller) {
  controller[kState].transformAlgorithm = undefined;
  controller[kState].flushAlgorithm = undefined;
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

function transformStreamDefaultControllerPerformTransform(controller, chunk) {
  const transformPromise =
    ensureIsPromise(
      controller[kState].transformAlgorithm,
      controller,
      chunk,
      controller);
  return PromisePrototypeCatch(
    transformPromise,
    (error) => {
      transformStreamError(controller[kState].stream, error);
      throw error;
    });
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
  const {
    writable,
    controller,
  } = stream[kState];
  assert(writable[kState].state === 'writable');
  if (stream[kState].backpressure) {
    const backpressureChange = stream[kState].backpressureChange.promise;
    return PromisePrototypeThen(
      backpressureChange,
      () => {
        const {
          writable,
        } = stream[kState];
        if (writable[kState].state === 'erroring')
          throw writable[kState].storedError;
        assert(writable[kState].state === 'writable');
        return transformStreamDefaultControllerPerformTransform(
          controller,
          chunk);
      });
  }
  return transformStreamDefaultControllerPerformTransform(controller, chunk);
}

async function transformStreamDefaultSinkAbortAlgorithm(stream, reason) {
  transformStreamError(stream, reason);
}

function transformStreamDefaultSinkCloseAlgorithm(stream) {
  const {
    readable,
    controller,
  } = stream[kState];

  const flushPromise =
    ensureIsPromise(
      controller[kState].flushAlgorithm,
      controller,
      controller);
  transformStreamDefaultControllerClearAlgorithms(controller);
  return PromisePrototypeThen(
    flushPromise,
    () => {
      if (readable[kState].state === 'errored')
        throw readable[kState].storedError;
      readableStreamDefaultControllerClose(readable[kState].controller);
    },
    (error) => {
      transformStreamError(stream, error);
      throw readable[kState].storedError;
    });
}

function transformStreamDefaultSourcePullAlgorithm(stream) {
  assert(stream[kState].backpressure);
  assert(stream[kState].backpressureChange.promise !== undefined);
  transformStreamSetBackpressure(stream, false);
  return stream[kState].backpressureChange.promise;
}

module.exports = {
  TransformStream,
  TransformStreamDefaultController,
  TransferredTransformStream,

  // Exported Brand Checks
  isTransformStream,
  isTransformStreamDefaultController,
};
