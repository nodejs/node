'use strict';

const {
  Array,
  ArrayBufferPrototypeGetByteLength,
  ArrayBufferPrototypeGetDetached,
  ArrayBufferPrototypeSlice,
  AsyncIteratorPrototype,
  DataViewPrototypeGetBuffer,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetByteOffset,
  FunctionPrototypeCall,
  MathMax,
  NumberIsNaN,
  ObjectFreeze,
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

const assert = require('internal/assert');

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

// Backing store for the spec's [[queue]]: a power-of-two ring buffer
// instead of a plain array. Entries are pushed at the tail and consumed
// at the head, so a plain array either moves every element or forces the
// engine to re-linearize on each shift, and the default controllers would
// additionally have to allocate a { value, size } wrapper object per
// chunk just to keep the pair together. Default readable/writable
// controller queues store each entry as (value, size) in two consecutive
// slots via the *Pair methods; the readable byte controller queue stores
// its chunk descriptor records in single slots via push/shift/peek. A
// given instance only ever uses one of the two access patterns, so
// head/tail stay aligned to the entry stride.
class Queue {
  constructor(listLength = 8) {
    this.head = 0;
    this.tail = 0;
    // Number of logical entries currently in the queue: (value, size)
    // pairs for default controller queues, descriptor records for byte
    // controller queues.
    this.length = 0;
    this.capacityMask = listLength - 1;
    this.list = new Array(listLength);
    this.dequeuedSize = 0;
  }

  // Single-slot entries (readable byte controller chunk records).

  push(entry) {
    const tail = this.tail;
    this.list[tail] = entry;
    this.tail = (tail + 1) & this.capacityMask;
    this.length++;
    if (this.tail === this.head)
      this.grow();
  }

  shift() {
    const head = this.head;
    const list = this.list;
    const entry = list[head];
    list[head] = undefined;
    this.head = (head + 1) & this.capacityMask;
    if (--this.length === 0)
      this.rewind();
    return entry;
  }

  peek() {
    return this.list[this.head];
  }

  // Two-slot (value, size) entries (default controller queues). The
  // stride is always 2 and capacities are even, so `tail + 1`/`head + 1`
  // never need to wrap.

  pushPair(value, size) {
    const tail = this.tail;
    const list = this.list;
    list[tail] = value;
    list[tail + 1] = size;
    this.tail = (tail + 2) & this.capacityMask;
    this.length++;
    if (this.tail === this.head)
      this.grow();
  }

  // Returns the dequeued value; the size of the same entry is left in
  // `this.dequeuedSize` so that callers can update [[queueTotalSize]]
  // without a per-entry wrapper object having to exist.
  shiftPair() {
    const head = this.head;
    const list = this.list;
    const value = list[head];
    this.dequeuedSize = list[head + 1];
    list[head] = undefined;
    list[head + 1] = undefined;
    this.head = (head + 2) & this.capacityMask;
    if (--this.length === 0)
      this.rewind();
    return value;
  }

  peekPairValue() {
    return this.list[this.head];
  }

  // The ring is completely full (the post-push tail caught up with the
  // head): double the capacity, re-linearizing from the head so index
  // arithmetic stays trivial.
  grow() {
    const list = this.list;
    const capacity = list.length;
    const head = this.head;
    if (head !== 0) {
      const relinearized = new Array(capacity * 2);
      let n = 0;
      for (let i = head; i < capacity; i++)
        relinearized[n++] = list[i];
      for (let i = 0; i < head; i++)
        relinearized[n++] = list[i];
      this.list = relinearized;
      this.head = 0;
    } else {
      list.length = capacity * 2;
    }
    this.tail = capacity;
    this.capacityMask = (capacity * 2) - 1;
  }

  // The queue just became empty: restart at slot 0 so shallow queues net
  // sequential slot access, and drop the enlarged backing store after a
  // large burst has fully drained.
  rewind() {
    this.head = 0;
    this.tail = 0;
    if (this.list.length > 1024) {
      this.list.length = 8;
      this.capacityMask = 0b111;
    }
  }
}

// Controllers start out with (and are reset to) this shared immutable
// empty queue, so constructing a stream never allocates queue storage;
// a real Queue is materialized by the enqueue paths on first use. All
// dequeue/peek paths are guarded by `.length` (or the equivalent
// [[queueTotalSize]]) checks, so they can never observe the sentinel in
// a mutating way; it never stores entries, so it gets a zero-length
// backing list.
const kEmptyQueue = ObjectFreeze(new Queue(0));

function materializeQueue(state) {
  const queue = state.queue;
  if (queue === kEmptyQueue)
    return state.queue = new Queue();
  return queue;
}

// The queue helpers below run once per chunk on the hot paths of every
// default readable/writable stream, so they load the controller state a
// single time and don't assert the existence of the queue fields (both
// are unconditionally initialized during controller setup and only ever
// replaced wholesale).
function dequeueValue(controller) {
  const state = controller[kState];
  const queue = state.queue;
  assert(queue.length);
  const value = queue.shiftPair();
  state.queueTotalSize = MathMax(0, state.queueTotalSize - queue.dequeuedSize);
  return value;
}

function resetQueue(controller) {
  const state = controller[kState];
  state.queue = kEmptyQueue;
  state.queueTotalSize = 0;
}

function peekQueueValue(controller) {
  const state = controller[kState];
  assert(state.queue.length);
  return state.queue.peekPairValue();
}

function enqueueValueWithSize(controller, value, size) {
  const state = controller[kState];
  const coercedSize = +size;
  if (NumberIsNaN(coercedSize) ||
      coercedSize < 0 ||
      coercedSize === Infinity) {
    throw new ERR_INVALID_ARG_VALUE.RangeError('size', size);
  }
  materializeQueue(state).pushPair(value, coercedSize);
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
  Queue,
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
  kEmptyQueue,
  kState,
  kType,
  lazyTransfer,
  materializeQueue,
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
