'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
  AsyncIteratorPrototype,
  FunctionPrototypeCall,
  MathMax,
  NumberIsNaN,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  Symbol,
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
  canCopyArrayBuffer,
  cloneAsUint8Array,
  constants: {
    kPending,
  },
  getArrayBufferView,
  getPromiseDetails,
} = internalBinding('util');

const {
  inspect,
} = require('util');

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

// The default size algorithm is never exposed to user code, so a single
// shared function avoids one closure allocation per stream.
const defaultSizeAlgorithm = () => 1;

function extractSizeAlgorithm(size) {
  if (size === undefined) return defaultSizeAlgorithm;
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

// getArrayBufferView, canCopyArrayBuffer, and cloneAsUint8Array are
// implemented in src/node_util.cc via direct V8 API calls. They are immune to
// user tampering of typed-array prototypes (matching the defensive behavior of
// the previous Reflect.get-based JS implementation) and faster on hot
// byte-stream paths.

function isBrandCheck(brand) {
  return (value) => {
    return value != null &&
           value[kState] !== undefined &&
           value[kType] === brand;
  };
}

// The queue helpers below run once per chunk on the hot paths of every
// default readable/writable stream, so they load the controller state a
// single time and don't assert the existence of the queue fields (both
// are unconditionally initialized during controller setup and only ever
// replaced wholesale).
function dequeueValue(controller) {
  const state = controller[kState];
  assert(state.queue.length);
  const {
    value,
    size,
  } = ArrayPrototypeShift(state.queue);
  state.queueTotalSize = MathMax(0, state.queueTotalSize - size);
  return value;
}

function resetQueue(controller) {
  const state = controller[kState];
  state.queue = [];
  state.queueTotalSize = 0;
}

function peekQueueValue(controller) {
  const state = controller[kState];
  assert(state.queue.length);
  return state.queue[0].value;
}

function enqueueValueWithSize(controller, value, size) {
  const state = controller[kState];
  const coercedSize = +size;
  if (NumberIsNaN(coercedSize) ||
      coercedSize < 0 ||
      coercedSize === Infinity) {
    throw new ERR_INVALID_ARG_VALUE.RangeError('size', size);
  }
  ArrayPrototypePush(state.queue, { value, size: coercedSize });
  state.queueTotalSize += coercedSize;
}

// Arity-specialized variants of the promise-callback wrapper. The generic
// rest-parameter + ReflectApply form allocated an arguments array on every
// invocation; these run on per-chunk hot paths (pull/write/transform), so
// each known call-site arity gets its own wrapper. The exact number of
// arguments passed through to the user callback is observable and must be
// preserved.
function createPromiseCallbackNoParams(name, fn, thisArg) {
  validateFunction(fn, name);
  return async () => FunctionPrototypeCall(fn, thisArg);
}

function createPromiseCallback1Param(name, fn, thisArg) {
  validateFunction(fn, name);
  return async (arg) => FunctionPrototypeCall(fn, thisArg, arg);
}

function createPromiseCallback2Params(name, fn, thisArg) {
  validateFunction(fn, name);
  return async (arg1, arg2) => FunctionPrototypeCall(fn, thisArg, arg1, arg2);
}

function isPromisePending(promise) {
  if (promise === undefined) return false;
  const details = getPromiseDetails(promise);
  return details?.[0] === kPending;
}

// Shared shapes for lazily-materialized { promise, resolve, reject }
// records whose settlement is already known.
function resolvedRecord() {
  return {
    promise: PromiseResolve(),
    resolve: undefined,
    reject: undefined,
  };
}

function rejectedHandledRecord(error) {
  const record = {
    promise: PromiseReject(error),
    resolve: undefined,
    reject: undefined,
  };
  setPromiseHandled(record.promise);
  return record;
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
  AsyncIterator,
  canCopyArrayBuffer,
  cloneAsUint8Array,
  copyArrayBuffer,
  createPromiseCallbackNoParams,
  createPromiseCallback1Param,
  createPromiseCallback2Params,
  customInspect,
  defaultSizeAlgorithm,
  dequeueValue,
  enqueueValueWithSize,
  extractHighWaterMark,
  extractSizeAlgorithm,
  getArrayBufferView,
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
  rejectedHandledRecord,
  resetQueue,
  resolvedRecord,
  setPromiseHandled,
};
