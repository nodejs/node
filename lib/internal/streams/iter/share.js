'use strict';

// New Streams API - Share
//
// Pull-model multi-consumer streaming. Shares a single source among
// multiple consumers with explicit buffering.

const {
  ArrayPrototypePush,
  PromisePrototypeThen,
  PromiseResolve,
  PromiseWithResolvers,
  SafeSet,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
  TypedArrayPrototypeGetByteLength,
} = primordials;

const {
  shareProtocol,
  shareSyncProtocol,
} = require('internal/streams/iter/types');

const {
  from,
  fromSync,
  isAsyncIterable,
  isSyncIterable,
} = require('internal/streams/iter/from');

const {
  pull: pullWithTransforms,
  pullSync: pullSyncWithTransforms,
} = require('internal/streams/iter/pull');

const {
  kMultiConsumerDefaultBudget,
  getMinCursor,
  hasProtocol,
  onSignalAbort,
  wrapError,
  parsePullArgs,
  validateBackpressure,
} = require('internal/streams/iter/utils');

const {
  RingBuffer,
} = require('internal/streams/iter/ringbuffer');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_RETURN_VALUE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const {
  validateAbortSignal,
  validateInteger,
  validateObject,
} = require('internal/validators');

// =============================================================================
// Async Share Implementation
// =============================================================================

class ShareImpl {
  #source;
  #options;
  #buffer = new RingBuffer();
  #bufferStart = 0;
  #consumers = new SafeSet();
  #sourceIterator = null;
  #sourceExhausted = false;
  #sourceError;
  #cancelled = false;
  #pulling = false;
  #pullWaiters = [];
  #cachedMinCursor = 0;
  #cachedMinCursorConsumers = 0;
  /** Cumulative byte size of buffered entries */
  #bufferedBytes = 0;

  constructor(source, options) {
    this.#source = source;
    this.#options = options;
  }

  get consumerCount() {
    return this.#consumers.size;
  }

  pull(...args) {
    const { transforms, options } = parsePullArgs(args);
    const rawConsumer = this.#createRawConsumer();

    if (transforms.length > 0 || options?.signal) {
      if (options) {
        return pullWithTransforms(rawConsumer, ...transforms, options);
      }
      return pullWithTransforms(rawConsumer, ...transforms);
    }
    return rawConsumer;
  }

  #createRawConsumer() {
    const state = {
      __proto__: null,
      cursor: this.#bufferStart,
      resolve: null,
      reject: null,
      detached: false,
      pendingNext: PromiseResolve(),
    };

    this.#consumers.add(state);
    if (this.#consumers.size === 1) {
      this.#cachedMinCursor = state.cursor;
      this.#cachedMinCursorConsumers = 1;
    } else if (state.cursor === this.#cachedMinCursor) {
      this.#cachedMinCursorConsumers++;
    } else {
      this.#recomputeMinCursor();
    }
    const self = this;

    return {
      __proto__: null,
      [SymbolAsyncIterator]() {
        const getNext = async () => {
          if (self.#sourceError !== undefined) {
            state.detached = true;
            self.#consumers.delete(state);
            throw self.#sourceError;
          }

          // Loop until we get data, source is exhausted, or
          // consumer is detached. Multiple consumers may be woken
          // after a single pull - those that find no data at their
          // cursor must re-pull rather than terminating prematurely.
          for (;;) {
            if (state.detached) {
              if (self.#sourceError !== undefined) throw self.#sourceError;
              return { __proto__: null, done: true, value: undefined };
            }

            if (self.#cancelled) {
              state.detached = true;
              self.#deleteConsumer(state);
              return { __proto__: null, done: true, value: undefined };
            }

            // Check if data is available in buffer
            const bufferIndex = state.cursor - self.#bufferStart;
            if (bufferIndex < self.#buffer.length) {
              const chunk = self.#buffer.get(bufferIndex);
              const cursor = state.cursor;
              state.cursor++;
              if (cursor === self.#cachedMinCursor &&
                  --self.#cachedMinCursorConsumers === 0) {
                self.#tryTrimBuffer();
              }
              return { __proto__: null, done: false, value: chunk };
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#deleteConsumer(state);
              if (self.#sourceError !== undefined) throw self.#sourceError;
              return { __proto__: null, done: true, value: undefined };
            }

            // Need to pull from source - check buffer limit
            const shouldBuffer = await self.#waitForBufferSpace();
            if (shouldBuffer === null) {
              state.detached = true;
              self.#deleteConsumer(state);
              if (self.#sourceError !== undefined) throw self.#sourceError;
              return { __proto__: null, done: true, value: undefined };
            }

            await self.#pullFromSource(!shouldBuffer);
          }
        };

        return {
          __proto__: null,
          next() {
            const next = PromisePrototypeThen(
              state.pendingNext,
              getNext,
              getNext);
            state.pendingNext =
              PromisePrototypeThen(next, undefined, () => {});
            return next;
          },

          async return() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            if (self.#deleteConsumer(state)) {
              self.#tryTrimBuffer();
            }
            return { __proto__: null, done: true, value: undefined };
          },

          async throw() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            if (self.#deleteConsumer(state)) {
              self.#tryTrimBuffer();
            }
            return { __proto__: null, done: true, value: undefined };
          },
        };
      },
    };
  }

  cancel(reason) {
    if (this.#cancelled) return;
    this.#cancelled = true;

    if (reason !== undefined) {
      this.#sourceError = reason;
    }

    if (this.#sourceIterator?.return) {
      PromisePrototypeThen(this.#sourceIterator.return(), undefined, () => {});
    }

    for (const consumer of this.#consumers) {
      if (consumer.resolve) {
        if (reason !== undefined) {
          consumer.reject?.(reason);
        } else {
          consumer.resolve({ __proto__: null, done: true, value: undefined });
        }
        consumer.resolve = null;
        consumer.reject = null;
      }
      consumer.detached = true;
    }
    this.#consumers.clear();

    for (let i = 0; i < this.#pullWaiters.length; i++) {
      this.#pullWaiters[i]();
    }
    this.#pullWaiters = [];
  }

  [SymbolDispose]() {
    this.cancel();
  }

  // Internal methods

  async #waitForBufferSpace() {
    while (this.#bufferedBytes >= this.#options.budget) {
      if (this.#cancelled ||
          this.#sourceError !== undefined ||
          this.#sourceExhausted) {
        return this.#cancelled ? null : true;
      }

      switch (this.#options.backpressure) {
        case 'strict':
          throw new ERR_OUT_OF_RANGE(
            'buffered bytes', `< ${this.#options.budget}`,
            this.#bufferedBytes);
        case 'unbounded': {
          const { promise, resolve } = PromiseWithResolvers();
          ArrayPrototypePush(this.#pullWaiters, resolve);
          await promise;
          break;
        }
        case 'drop-oldest':
          while (this.#bufferedBytes >= this.#options.budget &&
                 this.#buffer.length > 0) {
            const evicted = this.#buffer.shift();
            this.#bufferedBytes -= this.#batchByteSize(evicted);
            this.#bufferStart++;
          }
          for (const consumer of this.#consumers) {
            if (consumer.cursor < this.#bufferStart) {
              this.#deleteConsumerFromMin(consumer);
              consumer.cursor = this.#bufferStart;
            }
          }
          this.#recomputeMinCursor();
          return true;
        case 'drop-newest':
          return false;
      }
    }
    return true;
  }

  #pullFromSource(discard = false) {
    if (this.#sourceExhausted || this.#cancelled) {
      return PromiseResolve();
    }

    if (this.#pulling) {
      const { promise, resolve } = PromiseWithResolvers();
      ArrayPrototypePush(this.#pullWaiters, resolve);
      return promise;
    }

    this.#pulling = true;

    return (async () => {
      try {
        if (!this.#sourceIterator) {
          if (isAsyncIterable(this.#source)) {
            this.#sourceIterator =
              this.#source[SymbolAsyncIterator]();
          } else if (isSyncIterable(this.#source)) {
            const syncIterator =
              this.#source[SymbolIterator]();
            this.#sourceIterator = {
              __proto__: null,
              async next() {
                return syncIterator.next();
              },
              async return() {
                return syncIterator.return?.() ??
                       { __proto__: null, done: true, value: undefined };
              },
            };
          } else {
            throw new ERR_INVALID_ARG_TYPE(
              'source', ['AsyncIterable', 'Iterable'], this.#source);
          }
        }

        const result = await this.#sourceIterator.next();

        if (result.done) {
          this.#sourceExhausted = true;
        } else if (!discard) {
          this.#buffer.push(result.value);
          this.#bufferedBytes += this.#batchByteSize(result.value);
        }
      } catch (error) {
        this.#sourceError = wrapError(error);
        this.#sourceExhausted = true;
      } finally {
        this.#pulling = false;
        for (let i = 0; i < this.#pullWaiters.length; i++) {
          this.#pullWaiters[i]();
        }
        this.#pullWaiters = [];
      }
    })();
  }

  #tryTrimBuffer() {
    if (this.#cachedMinCursorConsumers === 0) {
      this.#recomputeMinCursor();
    }
    const trimCount = this.#cachedMinCursor - this.#bufferStart;
    if (trimCount > 0) {
      for (let i = 0; i < trimCount; i++) {
        const evicted = this.#buffer.get(i);
        this.#bufferedBytes -= this.#batchByteSize(evicted);
      }
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = this.#cachedMinCursor;
      for (let i = 0; i < this.#pullWaiters.length; i++) {
        this.#pullWaiters[i]();
      }
      this.#pullWaiters = [];
    }
  }

  #batchByteSize(batch) {
    let size = 0;
    for (let i = 0; i < batch.length; i++) {
      size += TypedArrayPrototypeGetByteLength(batch[i]);
    }
    return size;
  }

  #recomputeMinCursor() {
    const { minCursor, minCursorConsumers } = getMinCursor(
      this.#consumers, this.#bufferStart + this.#buffer.length);
    this.#cachedMinCursor = minCursor;
    this.#cachedMinCursorConsumers = minCursorConsumers;
  }

  #deleteConsumerFromMin(consumer) {
    if (consumer.cursor === this.#cachedMinCursor) {
      this.#cachedMinCursorConsumers--;
      return this.#cachedMinCursorConsumers === 0;
    }
    return false;
  }

  #deleteConsumer(consumer) {
    if (this.#consumers.delete(consumer)) {
      return this.#deleteConsumerFromMin(consumer);
    }
    return false;
  }
}

// =============================================================================
// Sync Share Implementation
// =============================================================================

class SyncShareImpl {
  #source;
  #options;
  #buffer = new RingBuffer();
  #bufferStart = 0;
  #consumers = new SafeSet();
  #sourceIterator = null;
  #sourceExhausted = false;
  #sourceError;
  #cancelled = false;
  #cachedMinCursor = 0;
  #cachedMinCursorConsumers = 0;
  /** Cumulative byte size of buffered entries */
  #bufferedBytes = 0;

  constructor(source, options) {
    this.#source = source;
    this.#options = options;
  }

  get consumerCount() {
    return this.#consumers.size;
  }

  pull(...transforms) {
    const rawConsumer = this.#createRawConsumer();

    if (transforms.length > 0) {
      return pullSyncWithTransforms(rawConsumer, ...transforms);
    }
    return rawConsumer;
  }

  #createRawConsumer() {
    const state = {
      __proto__: null,
      cursor: this.#bufferStart,
      detached: false,
    };

    this.#consumers.add(state);
    if (this.#consumers.size === 1) {
      this.#cachedMinCursor = state.cursor;
      this.#cachedMinCursorConsumers = 1;
    } else if (state.cursor === this.#cachedMinCursor) {
      this.#cachedMinCursorConsumers++;
    } else {
      this.#recomputeMinCursor();
    }
    const self = this;

    return {
      __proto__: null,
      [SymbolIterator]() {
        return {
          __proto__: null,
          next() {
            if (self.#sourceError !== undefined) {
              state.detached = true;
              self.#deleteConsumer(state);
              throw self.#sourceError;
            }
            if (state.detached) {
              return { __proto__: null, done: true, value: undefined };
            }
            if (self.#cancelled) {
              state.detached = true;
              self.#deleteConsumer(state);
              return { __proto__: null, done: true, value: undefined };
            }

            const bufferIndex = state.cursor - self.#bufferStart;
            if (bufferIndex < self.#buffer.length) {
              const chunk = self.#buffer.get(bufferIndex);
              const cursor = state.cursor;
              state.cursor++;
              if (cursor === self.#cachedMinCursor &&
                  --self.#cachedMinCursorConsumers === 0) {
                self.#tryTrimBuffer();
              }
              return { __proto__: null, done: false, value: chunk };
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#deleteConsumer(state);
              return { __proto__: null, done: true, value: undefined };
            }

            // Check buffer limit
            if (self.#bufferedBytes >= self.#options.budget) {
              switch (self.#options.backpressure) {
                case 'strict':
                  throw new ERR_OUT_OF_RANGE(
                    'buffered bytes', `< ${self.#options.budget}`,
                    self.#bufferedBytes);
                case 'unbounded':
                  throw new ERR_OUT_OF_RANGE(
                    'buffered bytes', `< ${self.#options.budget} ` +
                    '(unbounded not available in sync context)',
                    self.#bufferedBytes);
                case 'drop-oldest':
                  while (self.#bufferedBytes >= self.#options.budget &&
                         self.#buffer.length > 0) {
                    const evicted = self.#buffer.shift();
                    self.#bufferedBytes -= self.#batchByteSize(evicted);
                    self.#bufferStart++;
                  }
                  for (const consumer of self.#consumers) {
                    if (consumer.cursor < self.#bufferStart) {
                      self.#deleteConsumerFromMin(consumer);
                      consumer.cursor = self.#bufferStart;
                    }
                  }
                  self.#recomputeMinCursor();
                  break;
                case 'drop-newest':
                  state.detached = true;
                  self.#deleteConsumer(state);
                  return { __proto__: null, done: true, value: undefined };
              }
            }

            self.#pullFromSource();

            if (self.#sourceError !== undefined) {
              state.detached = true;
              self.#deleteConsumer(state);
              throw self.#sourceError;
            }

            const newBufferIndex = state.cursor - self.#bufferStart;
            if (newBufferIndex < self.#buffer.length) {
              const chunk = self.#buffer.get(newBufferIndex);
              const cursor = state.cursor;
              state.cursor++;
              if (cursor === self.#cachedMinCursor &&
                  --self.#cachedMinCursorConsumers === 0) {
                self.#tryTrimBuffer();
              }
              return { __proto__: null, done: false, value: chunk };
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#deleteConsumer(state);
              return { __proto__: null, done: true, value: undefined };
            }

            return { __proto__: null, done: true, value: undefined };
          },

          return() {
            state.detached = true;
            if (self.#deleteConsumer(state)) {
              self.#tryTrimBuffer();
            }
            return { __proto__: null, done: true, value: undefined };
          },

          throw() {
            state.detached = true;
            if (self.#deleteConsumer(state)) {
              self.#tryTrimBuffer();
            }
            return { __proto__: null, done: true, value: undefined };
          },
        };
      },
    };
  }

  cancel(reason) {
    if (this.#cancelled) return;
    this.#cancelled = true;

    if (reason !== undefined) {
      this.#sourceError = reason;
    }

    if (this.#sourceIterator?.return) {
      this.#sourceIterator.return();
    }

    for (const consumer of this.#consumers) {
      consumer.detached = true;
    }
    this.#consumers.clear();
  }

  [SymbolDispose]() {
    this.cancel();
  }

  #pullFromSource() {
    if (this.#sourceExhausted || this.#cancelled) return;

    try {
      this.#sourceIterator ||= this.#source[SymbolIterator]();

      const result = this.#sourceIterator.next();

      if (result.done) {
        this.#sourceExhausted = true;
      } else {
        this.#buffer.push(result.value);
        this.#bufferedBytes += this.#batchByteSize(result.value);
      }
    } catch (error) {
      this.#sourceError = wrapError(error);
      this.#sourceExhausted = true;
    }
  }

  #tryTrimBuffer() {
    if (this.#cachedMinCursorConsumers === 0) {
      this.#recomputeMinCursor();
    }
    const trimCount = this.#cachedMinCursor - this.#bufferStart;
    if (trimCount > 0) {
      for (let i = 0; i < trimCount; i++) {
        const evicted = this.#buffer.get(i);
        this.#bufferedBytes -= this.#batchByteSize(evicted);
      }
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = this.#cachedMinCursor;
    }
  }

  #batchByteSize(batch) {
    let size = 0;
    for (let i = 0; i < batch.length; i++) {
      size += TypedArrayPrototypeGetByteLength(batch[i]);
    }
    return size;
  }

  #recomputeMinCursor() {
    const { minCursor, minCursorConsumers } = getMinCursor(
      this.#consumers, this.#bufferStart + this.#buffer.length);
    this.#cachedMinCursor = minCursor;
    this.#cachedMinCursorConsumers = minCursorConsumers;
  }

  #deleteConsumerFromMin(consumer) {
    if (consumer.cursor === this.#cachedMinCursor) {
      this.#cachedMinCursorConsumers--;
      return this.#cachedMinCursorConsumers === 0;
    }
    return false;
  }

  #deleteConsumer(consumer) {
    if (this.#consumers.delete(consumer)) {
      return this.#deleteConsumerFromMin(consumer);
    }
    return false;
  }
}

function onShareCancel(shareImpl, signal) {
  onSignalAbort(signal, () => shareImpl.cancel(signal.reason));
}

// =============================================================================
// Public API
// =============================================================================

function share(source, options = { __proto__: null }) {
  // Normalize source via from() - accepts strings, ArrayBuffers, protocols, etc.
  const normalized = from(source);
  validateObject(options, 'options');
  const {
    budget = kMultiConsumerDefaultBudget,
    backpressure = 'strict',
    signal,
  } = options;
  validateInteger(budget, 'options.budget', 16384);
  validateBackpressure(backpressure);
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
  }

  const opts = {
    __proto__: null,
    budget,
    backpressure,
    signal,
  };

  const shareImpl = new ShareImpl(normalized, opts);

  if (signal) {
    onShareCancel(shareImpl, signal);
  }

  return shareImpl;
}

function shareSync(source, options = { __proto__: null }) {
  // Normalize source via fromSync() - accepts strings, ArrayBuffers, protocols, etc.
  const normalized = fromSync(source);
  validateObject(options, 'options');
  const {
    budget = kMultiConsumerDefaultBudget,
    backpressure = 'strict',
  } = options;
  validateInteger(budget, 'options.budget', 16384);
  validateBackpressure(backpressure);

  const opts = {
    __proto__: null,
    budget,
    backpressure,
  };

  return new SyncShareImpl(normalized, opts);
}

function isShareable(value) {
  return hasProtocol(value, shareProtocol);
}

function isSyncShareable(value) {
  return hasProtocol(value, shareSyncProtocol);
}

const Share = {
  __proto__: null,
  from(input, options) {
    if (isShareable(input)) {
      const result = input[shareProtocol](options);
      if (result === null || typeof result !== 'object') {
        throw new ERR_INVALID_RETURN_VALUE(
          'an object', '[Symbol.for(\'Stream.shareProtocol\')]', result);
      }
      return result;
    }
    if (isAsyncIterable(input) || isSyncIterable(input)) {
      return share(input, options);
    }
    throw new ERR_INVALID_ARG_TYPE(
      'input', ['Shareable', 'AsyncIterable', 'Iterable'], input);
  },
};

const SyncShare = {
  __proto__: null,
  fromSync(input, options) {
    if (isSyncShareable(input)) {
      const result = input[shareSyncProtocol](options);
      if (result === null || typeof result !== 'object') {
        throw new ERR_INVALID_RETURN_VALUE(
          'an object', '[Symbol.for(\'Stream.shareSyncProtocol\')]', result);
      }
      return result;
    }
    if (isSyncIterable(input)) {
      return shareSync(input, options);
    }
    throw new ERR_INVALID_ARG_TYPE(
      'input', ['SyncShareable', 'Iterable'], input);
  },
};

module.exports = {
  Share,
  SyncShare,
  share,
  shareSync,
};
