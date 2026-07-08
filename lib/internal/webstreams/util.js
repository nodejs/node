'use strict';

const {
  ArrayBufferPrototypeGetByteLength,
  ArrayBufferPrototypeGetDetached,
  ArrayBufferPrototypeSlice,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  AsyncIteratorPrototype,
  DataViewPrototypeGetBuffer,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetByteOffset,
  FunctionPrototypeCall,
  MathMax,
  NumberIsNaN,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  Symbol,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
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

const {
  isDataView,
} = require('internal/util/types');

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

// These use the original prototype getters so that user tampering with
// the buffer, byteLength, and byteOffset properties on ArrayBuffer and
// ArrayBufferView's is not observed. They run once or more per chunk on
// every byte-stream path, so they must not go through a reflective get
// (the previous view.constructor.prototype lookup was both slower and
// spoofable via a user-defined .constructor).

function ArrayBufferViewGetBuffer(view) {
  return isDataView(view) ?
    DataViewPrototypeGetBuffer(view) :
    TypedArrayPrototypeGetBuffer(view);
}

function ArrayBufferViewGetByteLength(view) {
  return isDataView(view) ?
    DataViewPrototypeGetByteLength(view) :
    TypedArrayPrototypeGetByteLength(view);
}

function ArrayBufferViewGetByteOffset(view) {
  return isDataView(view) ?
    DataViewPrototypeGetByteOffset(view) :
    TypedArrayPrototypeGetByteOffset(view);
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

// The queue helpers below run once per chunk on the hot paths of every
// default readable/writable stream, so they load the controller state a
// single time and don't assert the existence of the queue fields (both
// are unconditionally initialized during controller setup and only ever
// replaced wholesale).
//
// The queue is drained from a moving head index instead of with
// ArrayPrototypeShift: shifting is O(queue length), so draining a buffered
// queue one chunk at a time is O(n^2). Reading through `queueHead` makes
// each dequeue O(1); the consumed prefix is dropped in amortized O(1) once
// it grows to at least half of the backing array (and past a small floor,
// so a short oscillating queue never pays a per-dequeue array mutation).
// Callers must treat `queue.length - queueHead`, not `queue.length`, as the
// number of live entries.
function dequeueValue(controller) {
  const state = controller[kState];
  const queue = state.queue;
  const head = state.queueHead;
  const {
    value,
    size,
  } = queue[head];
  queue[head] = undefined;
  const newHead = head + 1;
  state.queueTotalSize = MathMax(0, state.queueTotalSize - size);
  if (newHead >= 32 && newHead * 2 >= queue.length) {
    state.queue = ArrayPrototypeSlice(queue, newHead);
    state.queueHead = 0;
  } else {
    state.queueHead = newHead;
  }
  return value;
}

function resetQueue(controller) {
  const state = controller[kState];
  state.queue = [];
  state.queueHead = 0;
  state.queueTotalSize = 0;
}

function peekQueueValue(controller) {
  const state = controller[kState];
  return state.queue[state.queueHead].value;
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
  ArrayBufferViewGetBuffer,
  ArrayBufferViewGetByteLength,
  ArrayBufferViewGetByteOffset,
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
