'use strict';

const {
  ArrayBufferPrototype,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  AsyncIteratorPrototype,
  FunctionPrototypeCall,
  MathMax,
  NumberIsNaN,
  ObjectCreate,
  PromisePrototypeThen,
  PromiseResolve,
  PromiseReject,
  ReflectGet,
  Symbol,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OPERATION_FAILED,
  },
} = require('internal/errors');

const {
  copyArrayBuffer,
  detachArrayBuffer
} = internalBinding('buffer');

const {
  isPromise,
} = require('internal/util/types');

const {
  inspect,
} = require('util');

const {
  getPromiseDetails,
  kPending,
} = internalBinding('util');

const assert = require('internal/assert');

const kState = Symbol('kState');
const kType = Symbol('kType');

const AsyncIterator = ObjectCreate(AsyncIteratorPrototype, {
  next: {
    configurable: true,
    enumerable: true,
    writable: true,
  },
  return: {
    configurable: true,
    enumerable: true,
    writable: true,
  },
});

function extractHighWaterMark(value, defaultHWM) {
  if (value === undefined) return defaultHWM;
  value = +value;
  if (typeof value !== 'number' ||
      NumberIsNaN(value) ||
      value < 0)
    throw new ERR_INVALID_ARG_VALUE.RangeError('strategy.highWaterMark', value);
  return value;
}

function extractSizeAlgorithm(size) {
  if (size === undefined) return () => 1;
  if (typeof size !== 'function')
    throw new ERR_INVALID_ARG_TYPE('strategy.size', 'Function', size);
  return size;
}

function customInspect(depth, options, name, data) {
  if (depth < 0)
    return this;

  const opts = {
    ...options,
    depth: options.depth == null ? null : options.depth - 1
  };

  return `${name} ${inspect(data, opts)}`;
}

// These are defensive to work around the possibility that
// the buffer, byteLength, and byteOffset properties on
// ArrayBuffer and ArrayBufferView's may have been tampered with.

function ArrayBufferViewGetBuffer(view) {
  return ReflectGet(view.constructor.prototype, 'buffer', view);
}

function ArrayBufferViewGetByteLength(view) {
  return ReflectGet(view.constructor.prototype, 'byteLength', view);
}

function ArrayBufferViewGetByteOffset(view) {
  return ReflectGet(view.constructor.prototype, 'byteOffset', view);
}

function ArrayBufferGetByteLength(view) {
  return ReflectGet(ArrayBufferPrototype, 'byteLength', view);
}

function isBrandCheck(brand) {
  return (value) => {
    return value != null &&
           value[kState] !== undefined &&
           value[kType] === brand;
  };
}

function transferArrayBuffer(buffer) {
  const res = detachArrayBuffer(buffer);
  if (res === undefined) {
    throw new ERR_OPERATION_FAILED.TypeError(
      'The ArrayBuffer could not be transferred');
  }
  return res;
}

function dequeueValue(controller) {
  assert(controller[kState].queue !== undefined);
  assert(controller[kState].queueTotalSize !== undefined);
  assert(controller[kState].queue.length);
  const {
    value,
    size,
  } = ArrayPrototypeShift(controller[kState].queue);
  controller[kState].queueTotalSize =
    MathMax(0, controller[kState].queueTotalSize - size);
  return value;
}

function resetQueue(controller) {
  assert(controller[kState].queue !== undefined);
  assert(controller[kState].queueTotalSize !== undefined);
  controller[kState].queue = [];
  controller[kState].queueTotalSize = 0;
}

function peekQueueValue(controller) {
  assert(controller[kState].queue !== undefined);
  assert(controller[kState].queueTotalSize !== undefined);
  assert(controller[kState].queue.length);
  return controller[kState].queue[0].value;
}

function enqueueValueWithSize(controller, value, size) {
  assert(controller[kState].queue !== undefined);
  assert(controller[kState].queueTotalSize !== undefined);
  size = +size;
  if (typeof size !== 'number' ||
      size < 0 ||
      NumberIsNaN(size) ||
      size === Infinity) {
    throw new ERR_INVALID_ARG_VALUE.RangeError('size', size);
  }
  ArrayPrototypePush(controller[kState].queue, { value, size });
  controller[kState].queueTotalSize += size;
}

function ensureIsPromise(fn, thisArg, ...args) {
  try {
    const value = FunctionPrototypeCall(fn, thisArg, ...args);
    return isPromise(value) ? value : PromiseResolve(value);
  } catch (error) {
    return PromiseReject(error);
  }
}

function isPromisePending(promise) {
  if (promise === undefined) return false;
  const details = getPromiseDetails(promise);
  return details?.[0] === kPending;
}

function setPromiseHandled(promise) {
  // Alternatively, we could use the native API
  // MarkAsHandled, but this avoids the extra boundary cross
  // and is hopefully faster at the cost of an extra Promise
  // allocation.
  PromisePrototypeThen(promise, () => {}, () => {});
}

async function nonOpFlush() {}

function nonOpStart() {}

async function nonOpPull() {}

async function nonOpCancel() {}

async function nonOpWrite() {}

let transfer;
function lazyTransfer() {
  if (transfer === undefined)
    transfer = require('internal/webstreams/transfer');
  return transfer;
}

const kEnumerableProperty = ObjectCreate(null);
kEnumerableProperty.enumerable = true;

module.exports = {
  ArrayBufferViewGetBuffer,
  ArrayBufferViewGetByteLength,
  ArrayBufferViewGetByteOffset,
  ArrayBufferGetByteLength,
  AsyncIterator,
  copyArrayBuffer,
  customInspect,
  dequeueValue,
  ensureIsPromise,
  enqueueValueWithSize,
  extractHighWaterMark,
  extractSizeAlgorithm,
  lazyTransfer,
  isBrandCheck,
  isPromisePending,
  peekQueueValue,
  resetQueue,
  setPromiseHandled,
  transferArrayBuffer,
  nonOpCancel,
  nonOpFlush,
  nonOpPull,
  nonOpStart,
  nonOpWrite,
  kType,
  kState,
  kEnumerableProperty,
};
