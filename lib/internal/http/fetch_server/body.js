'use strict';

// Body sources and async iteration shared by ServerRequest and
// ServerResponse.
// https://fetch-server.proposal.wintertc.org/#serverresponse-body

const {
  ArrayBufferIsView,
  ArrayBufferPrototypeSlice,
  ArrayPrototypePush,
  ArrayPrototypeShift,
  DataViewPrototypeGetBuffer,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetByteOffset,
  FunctionPrototypeCall,
  ObjectDefineProperty,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  Symbol,
  SymbolAsyncIterator,
  SymbolIterator,
  SymbolToStringTag,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  TypedArrayPrototypeSlice,
  Uint8Array,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const {
  isAnyArrayBuffer,
  isDataView,
  isUint8Array,
} = require('internal/util/types');

const {
  kEnumerableProperty,
} = require('internal/util');

const kAsyncIteratorPrototype = ObjectGetPrototypeOf(
  ObjectGetPrototypeOf(async function* () {}).prototype);

function noop() {}

function markHandled(promise) {
  PromisePrototypeThen(promise, undefined, noop);
  return promise;
}

// =============================================================================
// Body sources
// =============================================================================
// A body source implements:
//   read(): Promise<Uint8Array[] | null>  (null signals end of the body)
//   close(reason): void

class BytesSource {
  #bytes;
  constructor(bytes) {
    this.#bytes = bytes;
  }
  read() {
    const bytes = this.#bytes;
    this.#bytes = null;
    if (bytes === null || TypedArrayPrototypeGetByteLength(bytes) === 0) {
      return PromiseResolve(null);
    }
    return PromiseResolve([bytes]);
  }
  close() {
    this.#bytes = null;
  }

  /**
   * Takes the bytes synchronously, for sending them in a single write.
   * @returns {Uint8Array} The bytes, which can be empty.
   */
  take() {
    const bytes = this.#bytes ?? new Uint8Array(0);
    this.#bytes = null;
    return bytes;
  }
}

class ReadableStreamSource {
  #stream;
  #reader = null;
  #ended = false;
  constructor(stream) {
    this.#stream = stream;
  }
  async read() {
    if (this.#ended) return null;
    this.#reader ??= this.#stream.getReader();
    const { done, value } = await this.#reader.read();
    if (done) {
      this.#ended = true;
      return null;
    }
    if (!isUint8Array(value)) {
      this.#ended = true;
      const err = new ERR_INVALID_ARG_TYPE('chunk', 'Uint8Array', value);
      markHandled(this.#reader.cancel(err));
      throw err;
    }
    return [value];
  }
  close(reason) {
    if (this.#ended) return;
    this.#ended = true;
    if (this.#reader !== null) {
      markHandled(this.#reader.cancel(reason));
    } else {
      markHandled(this.#stream.cancel(reason));
    }
  }
}

function getAsyncIterator(object) {
  const method = object[SymbolAsyncIterator];
  if (method !== undefined && method !== null) {
    return FunctionPrototypeCall(method, object);
  }
  const syncMethod = object[SymbolIterator];
  const syncIterator = FunctionPrototypeCall(syncMethod, object);
  return {
    __proto__: null,
    async next() {
      const { done, value } = syncIterator.next();
      return { __proto__: null, value: await value, done };
    },
    async return(value) {
      if (typeof syncIterator.return === 'function') {
        const result = syncIterator.return(value);
        return { __proto__: null, value: await result?.value, done: true };
      }
      return { __proto__: null, value, done: true };
    },
  };
}

/**
 * Converts a value produced by an async sequence to a batch of Uint8Array.
 * @param {any} value
 * @returns {Uint8Array[]}
 */
function toBatch(value) {
  if (value === null || typeof value !== 'object' ||
      typeof value[SymbolIterator] !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('batch', 'Iterable<Uint8Array>', value);
  }
  const batch = [];
  for (const chunk of value) {
    if (!isUint8Array(chunk)) {
      throw new ERR_INVALID_ARG_TYPE('chunk', 'Uint8Array', chunk);
    }
    ArrayPrototypePush(batch, chunk);
  }
  return batch;
}

class AsyncSequenceSource {
  #object;
  #iterator = null;
  #ended = false;
  constructor(object) {
    this.#object = object;
  }
  async read() {
    if (this.#ended) return null;
    this.#iterator ??= getAsyncIterator(this.#object);
    for (;;) {
      let result;
      try {
        result = await this.#iterator.next();
      } catch (err) {
        this.#ended = true;
        throw err;
      }
      if (result === null || typeof result !== 'object') {
        this.#ended = true;
        throw new ERR_INVALID_ARG_TYPE('result', 'Object', result);
      }
      if (result.done) {
        this.#ended = true;
        return null;
      }
      let batch;
      try {
        batch = toBatch(result.value);
      } catch (err) {
        this.close(err);
        throw err;
      }
      if (batch.length > 0) return batch;
    }
  }
  close(reason) {
    if (this.#ended) return;
    this.#ended = true;
    const iterator = this.#iterator;
    if (iterator !== null && typeof iterator.return === 'function') {
      try {
        const result = FunctionPrototypeCall(iterator.return, iterator, reason);
        if (result != null && typeof result.then === 'function') {
          markHandled(PromiseResolve(result));
        }
      } catch {
        // Errors closing the iterator are ignored.
      }
    }
  }
}

class LazySource {
  // Wraps a source that has to be created asynchronously (e.g. from a Blob).
  #factory;
  #source = null;
  #closed = false;
  constructor(factory) {
    this.#factory = factory;
  }
  async read() {
    if (this.#closed) return null;
    this.#source ??= await this.#factory();
    return this.#source.read();
  }
  close(reason) {
    if (this.#closed) return;
    this.#closed = true;
    this.#source?.close(reason);
  }
}

class SplitSide {
  #splitter;
  #queue = [];
  #closed = false;
  constructor(splitter) {
    this.#splitter = splitter;
  }
  get closed() {
    return this.#closed;
  }
  enqueue(entry) {
    ArrayPrototypePush(this.#queue, entry);
  }
  async read() {
    if (this.#closed) return null;
    while (this.#queue.length === 0) {
      await this.#splitter.pull();
    }
    const entry = this.#queue[0];
    if (entry.end) return null;
    if (entry.error !== undefined) throw entry.error;
    ArrayPrototypeShift(this.#queue);
    return entry.batch;
  }
  close(reason) {
    if (this.#closed) return;
    this.#closed = true;
    this.#queue = [];
    this.#splitter.sideClosed(reason);
  }
}

// Splitting a body for clone() (Section 9.5).
class Splitter {
  #source;
  #sides = [null, null];
  #reading = null;
  #closedCount = 0;
  constructor(source) {
    this.#source = source;
    this.#sides[0] = new SplitSide(this, 0);
    this.#sides[1] = new SplitSide(this, 1);
  }
  get sides() {
    return this.#sides;
  }
  pull() {
    if (this.#reading !== null) return this.#reading;
    this.#reading = PromisePrototypeThen(
      this.#source.read(),
      (batch) => {
        this.#reading = null;
        for (let i = 0; i < 2; i++) {
          const side = this.#sides[i];
          if (side.closed) continue;
          if (batch === null) {
            side.enqueue({ __proto__: null, end: true });
          } else if (i === 0) {
            side.enqueue({ __proto__: null, batch });
          } else {
            const copy = [];
            for (let j = 0; j < batch.length; j++) {
              ArrayPrototypePush(copy, TypedArrayPrototypeSlice(batch[j]));
            }
            side.enqueue({ __proto__: null, batch: copy });
          }
        }
      },
      (error) => {
        this.#reading = null;
        for (let i = 0; i < 2; i++) {
          const side = this.#sides[i];
          if (!side.closed) side.enqueue({ __proto__: null, error });
        }
      });
    return this.#reading;
  }
  sideClosed(reason) {
    if (++this.#closedCount === 2) {
      this.#source.close(reason);
    }
  }
}

function splitSource(source) {
  return new Splitter(source).sides;
}

function copyBufferSource(object) {
  if (isAnyArrayBuffer(object)) {
    return new Uint8Array(ArrayBufferPrototypeSlice(object, 0));
  }
  if (isDataView(object)) {
    return TypedArrayPrototypeSlice(new Uint8Array(
      DataViewPrototypeGetBuffer(object),
      DataViewPrototypeGetByteOffset(object),
      DataViewPrototypeGetByteLength(object)));
  }
  if (ArrayBufferIsView(object)) {
    return TypedArrayPrototypeSlice(new Uint8Array(
      TypedArrayPrototypeGetBuffer(object),
      TypedArrayPrototypeGetByteOffset(object),
      TypedArrayPrototypeGetByteLength(object)));
  }
  throw new ERR_INVALID_ARG_TYPE('body', 'BufferSource', object);
}

// =============================================================================
// Web IDL-style asynchronous iterators
// =============================================================================

/**
 * Creates a prototype for async iterator objects, as for a Web IDL
 * `async_iterable` declaration.
 * @param {string} name The interface name.
 * @returns {object}
 */
function createAsyncIteratorPrototype(name) {
  const proto = {
    __proto__: kAsyncIteratorPrototype,
    next() {
      const state = this?.[kIteratorState];
      if (state === undefined) {
        return PromiseReject(new ERR_INVALID_STATE.TypeError(
          `Value of "this" must be a ${name} AsyncIterator`));
      }
      const nextSteps = () => {
        if (state.finished) return { value: undefined, done: true };
        return PromisePrototypeThen(state.next(), (batch) => {
          if (batch === null) {
            state.finished = true;
            return { value: undefined, done: true };
          }
          return { value: batch, done: false };
        }, (err) => {
          state.finished = true;
          throw err;
        });
      };
      state.ongoing = state.ongoing !== null ?
        PromisePrototypeThen(state.ongoing, nextSteps, nextSteps) :
        PromiseResolve(nextSteps());
      return state.ongoing;
    },
    return(value) {
      const state = this?.[kIteratorState];
      if (state === undefined) {
        return PromiseReject(new ERR_INVALID_STATE.TypeError(
          `Value of "this" must be a ${name} AsyncIterator`));
      }
      const returnSteps = () => {
        if (state.finished) return { value, done: true };
        state.finished = true;
        return PromisePrototypeThen(
          PromiseResolve(state.return(value)),
          () => ({ value, done: true }));
      };
      state.ongoing = state.ongoing !== null ?
        PromisePrototypeThen(state.ongoing, returnSteps, returnSteps) :
        PromiseResolve(returnSteps());
      return state.ongoing;
    },
  };
  ObjectDefineProperty(proto, 'next', kEnumerableProperty);
  ObjectDefineProperty(proto, 'return', kEnumerableProperty);
  ObjectDefineProperty(proto, SymbolToStringTag, {
    __proto__: null,
    configurable: true,
    value: `${name} AsyncIterator`,
  });
  return proto;
}

const kIteratorState = Symbol('kIteratorState');

/**
 * @param {object} proto An async iterator prototype.
 * @param {() => Promise<Uint8Array[]|null>} next
 * @param {(value: any) => any} returnSteps
 * @returns {object}
 */
function createAsyncIterator(proto, next, returnSteps) {
  const iterator = {
    [kIteratorState]: {
      __proto__: null,
      next,
      return: returnSteps,
      ongoing: null,
      finished: false,
    },
  };
  ObjectSetPrototypeOf(iterator, proto);
  return iterator;
}

module.exports = {
  AsyncSequenceSource,
  BytesSource,
  LazySource,
  ReadableStreamSource,
  copyBufferSource,
  createAsyncIterator,
  createAsyncIteratorPrototype,
  markHandled,
  noop,
  splitSource,
  toBatch,
};
