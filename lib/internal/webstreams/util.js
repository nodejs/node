'use strict';

const {
  ArrayBufferPrototypeGetByteLength,
  ArrayBufferPrototypeGetDetached,
  ArrayBufferPrototypeSlice,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  AsyncIteratorPrototype,
  MathMax,
  NumberIsNaN,
  PromisePrototypeThen,
  ReflectApply,
  ReflectGet,
  Symbol,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');

const {
  copyArrayBuffer,
} = internalBinding('buffer');

const {
  inspect,
} = require('util');

const {
  constants: {
    kPending,
  },
  getPromiseDetails,
} = internalBinding('util');

const assert = require('internal/assert');

const {
  validateFunction,
} = require('internal/validators');

const kState = Symbol('kState');
const kType = Symbol('kType');

const AsyncIterator = {
  __proto__: AsyncIteratorPrototype,
  next: undefined,
  return: undefined,
};

const getNonWritablePropertyDescriptor = (value) => {
  return {
    __proto__: null,
    configurable: true,
    value,
  };
};

function extractHighWaterMark(value, defaultHWM) {
  if (value === undefined) return defaultHWM;
  const coercedValue = +value;
  if (NumberIsNaN(coercedValue) ||
      coercedValue < 0)
    throw new ERR_INVALID_ARG_VALUE.RangeError('strategy.highWaterMark', value);
  return coercedValue;
}

function extractSizeAlgorithm(size) {
  if (size === undefined) return () => 1;
  validateFunction(size, 'strategy.size');
  return size;
}

function customInspect(depth, options, name, data) {
  if (depth < 0)
    return this;

  const opts = {
    ...options,
    depth: options.depth == null ? null : options.depth - 1,
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

function cloneAsUint8Array(view) {
  const buffer = ArrayBufferViewGetBuffer(view);
  const byteOffset = ArrayBufferViewGetByteOffset(view);
  const byteLength = ArrayBufferViewGetByteLength(view);
  return new Uint8Array(
    ArrayBufferPrototypeSlice(buffer, byteOffset, byteOffset + byteLength),
  );
}

function canCopyArrayBuffer(toBuffer, toIndex, fromBuffer, fromIndex, count) {
  return toBuffer !== fromBuffer &&
    !ArrayBufferPrototypeGetDetached(toBuffer) &&
    !ArrayBufferPrototypeGetDetached(fromBuffer) &&
    toIndex + count <= ArrayBufferPrototypeGetByteLength(toBuffer) &&
    fromIndex + count <= ArrayBufferPrototypeGetByteLength(fromBuffer);
}

function isBrandCheck(brand) {
  return (value) => {
    return value != null &&
           value[kState] !== undefined &&
           value[kType] === brand;
  };
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
  const coercedSize = +size;
  if (NumberIsNaN(coercedSize) ||
      coercedSize < 0 ||
      coercedSize === Infinity) {
    throw new ERR_INVALID_ARG_VALUE.RangeError('size', size);
  }
  size = coercedSize;
  ArrayPrototypePush(controller[kState].queue, { value, size });
  controller[kState].queueTotalSize += size;
}

function createPromiseCallback(name, fn, thisArg) {
  validateFunction(fn, name);
  return async (...args) => ReflectApply(fn, thisArg, args);
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
  PromisePrototypeThen(promise, undefined, () => {});
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

module.exports = {
  ArrayBufferViewGetBuffer,
  ArrayBufferViewGetByteLength,
  ArrayBufferViewGetByteOffset,
  AsyncIterator,
  canCopyArrayBuffer,
  cloneAsUint8Array,
  copyArrayBuffer,
  createPromiseCallback,
  customInspect,
  dequeueValue,
  enqueueValueWithSize,
  extractHighWaterMark,
  extractSizeAlgorithm,
  getNonWritablePropertyDescriptor,
  isBrandCheck,
  isPromisePending,
  kState,
  kType,
  lazyTransfer,
  nonOpCancel,
  nonOpFlush,
  nonOpPull,
  nonOpStart,
  nonOpWrite,
  peekQueueValue,
  resetQueue,
  setPromiseHandled,
};
