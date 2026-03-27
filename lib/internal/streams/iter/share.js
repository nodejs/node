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
  kMultiConsumerDefaultHWM,
  clampHWM,
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
  #sourceError = null;
  #cancelled = false;
  #pulling = false;
  #pullWaiters = [];

  constructor(source, options) {
    this.#source = source;
    this.#options = options;
  }

  get consumerCount() {
    return this.#consumers.size;
  }

  get bufferSize() {
    return this.#buffer.length;
  }

  pull(...args) {
    const { transforms, options } = parsePullArgs(args);
    const rawConsumer = this.#createRawConsumer();

    if (transforms.length > 0) {
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
    };

    this.#consumers.add(state);
    const self = this;

    return {
      __proto__: null,
      [SymbolAsyncIterator]() {
        return {
          __proto__: null,
          async next() {
            if (self.#sourceError) {
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
                return { __proto__: null, done: true, value: undefined };
              }

              if (self.#cancelled) {
                state.detached = true;
                self.#consumers.delete(state);
                return { __proto__: null, done: true, value: undefined };
              }

              // Check if data is available in buffer
              const bufferIndex = state.cursor - self.#bufferStart;
              if (bufferIndex < self.#buffer.length) {
                const chunk = self.#buffer.get(bufferIndex);
                state.cursor++;
                self.#tryTrimBuffer();
                return { __proto__: null, done: false, value: chunk };
              }

              if (self.#sourceExhausted) {
                state.detached = true;
                self.#consumers.delete(state);
                if (self.#sourceError) throw self.#sourceError;
                return { __proto__: null, done: true, value: undefined };
              }

              // Need to pull from source - check buffer limit
              const canPull = await self.#waitForBufferSpace();
              if (!canPull) {
                state.detached = true;
                self.#consumers.delete(state);
                if (self.#sourceError) throw self.#sourceError;
                return { __proto__: null, done: true, value: undefined };
              }

              await self.#pullFromSource();
            }
          },

          async return() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            self.#consumers.delete(state);
            self.#tryTrimBuffer();
            return { __proto__: null, done: true, value: undefined };
          },

          async throw() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            self.#consumers.delete(state);
            self.#tryTrimBuffer();
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
    while (this.#buffer.length >= this.#options.highWaterMark) {
      if (this.#cancelled || this.#sourceError || this.#sourceExhausted) {
        return !this.#cancelled;
      }

      switch (this.#options.backpressure) {
        case 'strict':
          throw new ERR_OUT_OF_RANGE(
            'buffer size', `<= ${this.#options.highWaterMark}`,
            this.#buffer.length);
        case 'block': {
          const { promise, resolve } = PromiseWithResolvers();
          ArrayPrototypePush(this.#pullWaiters, resolve);
          await promise;
          break;
        }
        case 'drop-oldest':
          this.#buffer.shift();
          this.#bufferStart++;
          for (const consumer of this.#consumers) {
            if (consumer.cursor < this.#bufferStart) {
              consumer.cursor = this.#bufferStart;
            }
          }
          return true;
        case 'drop-newest':
          return true;
      }
    }
    return true;
  }

  #pullFromSource() {
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
        } else {
          this.#buffer.push(result.value);
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
    const minCursor = getMinCursor(
      this.#consumers, this.#bufferStart + this.#buffer.length);
    const trimCount = minCursor - this.#bufferStart;
    if (trimCount > 0) {
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = minCursor;
      for (let i = 0; i < this.#pullWaiters.length; i++) {
        this.#pullWaiters[i]();
      }
      this.#pullWaiters = [];
    }
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
  #sourceError = null;
  #cancelled = false;

  constructor(source, options) {
    this.#source = source;
    this.#options = options;
  }

  get consumerCount() {
    return this.#consumers.size;
  }

  get bufferSize() {
    return this.#buffer.length;
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
    const self = this;

    return {
      __proto__: null,
      [SymbolIterator]() {
        return {
          __proto__: null,
          next() {
            if (state.detached) {
              return { __proto__: null, done: true, value: undefined };
            }
            if (self.#sourceError) {
              state.detached = true;
              self.#consumers.delete(state);
              throw self.#sourceError;
            }
            if (self.#cancelled) {
              state.detached = true;
              self.#consumers.delete(state);
              return { __proto__: null, done: true, value: undefined };
            }

            const bufferIndex = state.cursor - self.#bufferStart;
            if (bufferIndex < self.#buffer.length) {
              const chunk = self.#buffer.get(bufferIndex);
              state.cursor++;
              self.#tryTrimBuffer();
              return { __proto__: null, done: false, value: chunk };
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#consumers.delete(state);
              return { __proto__: null, done: true, value: undefined };
            }

            // Check buffer limit
            if (self.#buffer.length >= self.#options.highWaterMark) {
              switch (self.#options.backpressure) {
                case 'strict':
                  throw new ERR_OUT_OF_RANGE(
                    'buffer size', `<= ${self.#options.highWaterMark}`,
                    self.#buffer.length);
                case 'block':
                  throw new ERR_OUT_OF_RANGE(
                    'buffer size', `<= ${self.#options.highWaterMark} ` +
                    '(blocking not available in sync context)',
                    self.#buffer.length);
                case 'drop-oldest':
                  self.#buffer.shift();
                  self.#bufferStart++;
                  for (const consumer of self.#consumers) {
                    if (consumer.cursor < self.#bufferStart) {
                      consumer.cursor = self.#bufferStart;
                    }
                  }
                  break;
                case 'drop-newest':
                  state.detached = true;
                  self.#consumers.delete(state);
                  return { __proto__: null, done: true, value: undefined };
              }
            }

            self.#pullFromSource();

            if (self.#sourceError) {
              state.detached = true;
              self.#consumers.delete(state);
              throw self.#sourceError;
            }

            const newBufferIndex = state.cursor - self.#bufferStart;
            if (newBufferIndex < self.#buffer.length) {
              const chunk = self.#buffer.get(newBufferIndex);
              state.cursor++;
              self.#tryTrimBuffer();
              return { __proto__: null, done: false, value: chunk };
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#consumers.delete(state);
              return { __proto__: null, done: true, value: undefined };
            }

            return { __proto__: null, done: true, value: undefined };
          },

          return() {
            state.detached = true;
            self.#consumers.delete(state);
            self.#tryTrimBuffer();
            return { __proto__: null, done: true, value: undefined };
          },

          throw() {
            state.detached = true;
            self.#consumers.delete(state);
            self.#tryTrimBuffer();
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
      }
    } catch (error) {
      this.#sourceError = wrapError(error);
      this.#sourceExhausted = true;
    }
  }

  #tryTrimBuffer() {
    const minCursor = getMinCursor(
      this.#consumers, this.#bufferStart + this.#buffer.length);
    const trimCount = minCursor - this.#bufferStart;
    if (trimCount > 0) {
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = minCursor;
    }
  }
}

// =============================================================================
// Public API
// =============================================================================

function share(source, options = { __proto__: null }) {
  // Normalize source via from() - accepts strings, ArrayBuffers, protocols, etc.
  const normalized = from(source);
  validateObject(options, 'options');
  const {
    highWaterMark = kMultiConsumerDefaultHWM,
    backpressure = 'strict',
    signal,
  } = options;
  validateInteger(highWaterMark, 'options.highWaterMark');
  validateBackpressure(backpressure);
  if (signal !== undefined) {
    validateAbortSignal(signal, 'options.signal');
  }

  const opts = {
    __proto__: null,
    highWaterMark: clampHWM(highWaterMark),
    backpressure,
    signal,
  };

  const shareImpl = new ShareImpl(normalized, opts);

  if (signal) {
    onSignalAbort(signal, () => shareImpl.cancel());
  }

  return shareImpl;
}

function shareSync(source, options = { __proto__: null }) {
  // Normalize source via fromSync() - accepts strings, ArrayBuffers, protocols, etc.
  const normalized = fromSync(source);
  validateObject(options, 'options');
  const {
    highWaterMark = kMultiConsumerDefaultHWM,
    backpressure = 'strict',
  } = options;
  validateInteger(highWaterMark, 'options.highWaterMark');
  validateBackpressure(backpressure);

  const opts = {
    __proto__: null,
    highWaterMark: clampHWM(highWaterMark),
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
