'use strict';

// New Streams API - Share
//
// Pull-model multi-consumer streaming. Shares a single source among
// multiple consumers with explicit buffering.

const {
  ArrayPrototypePush,
  Error,
  MathMax,
  Promise,
  PromiseResolve,
  SafeSet,
  String,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
} = primordials;

const {
  shareProtocol,
  shareSyncProtocol,
} = require('internal/streams/new/types');

const {
  isAsyncIterable,
  isSyncIterable,
} = require('internal/streams/new/from');

const {
  pull: pullWithTransforms,
  pullSync: pullSyncWithTransforms,
} = require('internal/streams/new/pull');

const {
  parsePullArgs,
} = require('internal/streams/new/utils');

const {
  RingBuffer,
} = require('internal/streams/new/ringbuffer');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_OPERATION_FAILED,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

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
      cursor: this.#bufferStart,
      resolve: null,
      reject: null,
      detached: false,
    };

    this.#consumers.add(state);
    const self = this;

    return {
      [SymbolAsyncIterator]() {
        return {
          async next() {
            if (self.#sourceError) {
              state.detached = true;
              self.#consumers.delete(state);
              throw self.#sourceError;
            }

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
              return { __proto__: null, done: true, value: undefined };
            }

            // Need to pull from source - check buffer limit
            const canPull = await self.#waitForBufferSpace(state);
            if (!canPull) {
              state.detached = true;
              self.#consumers.delete(state);
              if (self.#sourceError) throw self.#sourceError;
              return { __proto__: null, done: true, value: undefined };
            }

            await self.#pullFromSource();

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

    if (reason) {
      this.#sourceError = reason;
    }

    if (this.#sourceIterator?.return) {
      this.#sourceIterator.return().catch(() => {});
    }

    for (const consumer of this.#consumers) {
      if (consumer.resolve) {
        if (reason) {
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

  async #waitForBufferSpace(_state) {
    while (this.#buffer.length >= this.#options.highWaterMark) {
      if (this.#cancelled || this.#sourceError || this.#sourceExhausted) {
        return !this.#cancelled;
      }

      switch (this.#options.backpressure) {
        case 'strict':
          throw new ERR_OUT_OF_RANGE(
            'buffer size', `<= ${this.#options.highWaterMark}`,
            this.#buffer.length);
        case 'block':
          await new Promise((resolve) => {
            ArrayPrototypePush(this.#pullWaiters, resolve);
          });
          break;
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
      return new Promise((resolve) => {
        ArrayPrototypePush(this.#pullWaiters, resolve);
      });
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
              async next() {
                return syncIterator.next();
              },
              async return() {
                return syncIterator.return?.() ??
                       { done: true, value: undefined };
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
        this.#sourceError =
          error instanceof Error ? error : new ERR_OPERATION_FAILED(String(error));
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

  #getMinCursor() {
    let min = Infinity;
    for (const consumer of this.#consumers) {
      if (consumer.cursor < min) {
        min = consumer.cursor;
      }
    }
    return min === Infinity ?
      this.#bufferStart + this.#buffer.length : min;
  }

  #tryTrimBuffer() {
    const minCursor = this.#getMinCursor();
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
      cursor: this.#bufferStart,
      detached: false,
    };

    this.#consumers.add(state);
    const self = this;

    return {
      [SymbolIterator]() {
        return {
          next() {
            if (state.detached) {
              return { done: true, value: undefined };
            }
            if (self.#sourceError) {
              state.detached = true;
              self.#consumers.delete(state);
              throw self.#sourceError;
            }
            if (self.#cancelled) {
              state.detached = true;
              self.#consumers.delete(state);
              return { done: true, value: undefined };
            }

            const bufferIndex = state.cursor - self.#bufferStart;
            if (bufferIndex < self.#buffer.length) {
              const chunk = self.#buffer.get(bufferIndex);
              state.cursor++;
              self.#tryTrimBuffer();
              return { done: false, value: chunk };
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#consumers.delete(state);
              return { done: true, value: undefined };
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
                  return { done: true, value: undefined };
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
              return { done: false, value: chunk };
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#consumers.delete(state);
              return { done: true, value: undefined };
            }

            return { done: true, value: undefined };
          },

          return() {
            state.detached = true;
            self.#consumers.delete(state);
            self.#tryTrimBuffer();
            return { done: true, value: undefined };
          },

          throw() {
            state.detached = true;
            self.#consumers.delete(state);
            self.#tryTrimBuffer();
            return { done: true, value: undefined };
          },
        };
      },
    };
  }

  cancel(reason) {
    if (this.#cancelled) return;
    this.#cancelled = true;

    if (reason) {
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
      this.#sourceError =
        error instanceof Error ? error : new ERR_OPERATION_FAILED(String(error));
      this.#sourceExhausted = true;
    }
  }

  #getMinCursor() {
    let min = Infinity;
    for (const consumer of this.#consumers) {
      if (consumer.cursor < min) {
        min = consumer.cursor;
      }
    }
    return min === Infinity ?
      this.#bufferStart + this.#buffer.length : min;
  }

  #tryTrimBuffer() {
    const minCursor = this.#getMinCursor();
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

function share(source, options) {
  const opts = {
    highWaterMark: MathMax(1, options?.highWaterMark ?? 16),
    backpressure: options?.backpressure ?? 'strict',
    signal: options?.signal,
  };

  const shareImpl = new ShareImpl(source, opts);

  if (opts.signal) {
    if (opts.signal.aborted) {
      shareImpl.cancel();
    } else {
      opts.signal.addEventListener('abort', () => {
        shareImpl.cancel();
      }, { once: true });
    }
  }

  return shareImpl;
}

function shareSync(source, options) {
  const opts = {
    highWaterMark: MathMax(1, options?.highWaterMark ?? 16),
    backpressure: options?.backpressure ?? 'strict',
  };

  return new SyncShareImpl(source, opts);
}

function isShareable(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    shareProtocol in value &&
    typeof value[shareProtocol] === 'function'
  );
}

function isSyncShareable(value) {
  return (
    value !== null &&
    typeof value === 'object' &&
    shareSyncProtocol in value &&
    typeof value[shareSyncProtocol] === 'function'
  );
}

const Share = {
  from(input, options) {
    if (isShareable(input)) {
      return input[shareProtocol](options);
    }
    if (isAsyncIterable(input) || isSyncIterable(input)) {
      return share(input, options);
    }
    throw new ERR_INVALID_ARG_TYPE(
      'input', ['Shareable', 'AsyncIterable', 'Iterable'], input);
  },
};

const SyncShare = {
  fromSync(input, options) {
    if (isSyncShareable(input)) {
      return input[shareSyncProtocol](options);
    }
    if (isSyncIterable(input)) {
      return shareSync(input, options);
    }
    throw new ERR_INVALID_ARG_TYPE(
      'input', ['SyncShareable', 'Iterable'], input);
  },
};

module.exports = {
  share,
  shareSync,
  Share,
  SyncShare,
};
