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
  constructor(source, options) {
    this._source = source;
    this._options = options;
    this._buffer = new RingBuffer();
    this._bufferStart = 0;
    this._consumers = new SafeSet();
    this._sourceIterator = null;
    this._sourceExhausted = false;
    this._sourceError = null;
    this._cancelled = false;
    this._pulling = false;
    this._pullWaiters = [];
  }

  get consumerCount() {
    return this._consumers.size;
  }

  get bufferSize() {
    return this._buffer.length;
  }

  pull(...args) {
    const { transforms, options } = parsePullArgs(args);
    const rawConsumer = this._createRawConsumer();

    if (transforms.length > 0) {
      if (options) {
        return pullWithTransforms(rawConsumer, ...transforms, options);
      }
      return pullWithTransforms(rawConsumer, ...transforms);
    }
    return rawConsumer;
  }

  _createRawConsumer() {
    const state = {
      cursor: this._bufferStart,
      resolve: null,
      reject: null,
      detached: false,
    };

    this._consumers.add(state);
    const self = this;

    return {
      [SymbolAsyncIterator]() {
        return {
          async next() {
            if (self._sourceError) {
              state.detached = true;
              self._consumers.delete(state);
              throw self._sourceError;
            }

            if (state.detached) {
              return { __proto__: null, done: true, value: undefined };
            }

            if (self._cancelled) {
              state.detached = true;
              self._consumers.delete(state);
              return { __proto__: null, done: true, value: undefined };
            }

            // Check if data is available in buffer
            const bufferIndex = state.cursor - self._bufferStart;
            if (bufferIndex < self._buffer.length) {
              const chunk = self._buffer.get(bufferIndex);
              state.cursor++;
              self._tryTrimBuffer();
              return { __proto__: null, done: false, value: chunk };
            }

            if (self._sourceExhausted) {
              state.detached = true;
              self._consumers.delete(state);
              return { __proto__: null, done: true, value: undefined };
            }

            // Need to pull from source - check buffer limit
            const canPull = await self._waitForBufferSpace(state);
            if (!canPull) {
              state.detached = true;
              self._consumers.delete(state);
              if (self._sourceError) throw self._sourceError;
              return { __proto__: null, done: true, value: undefined };
            }

            await self._pullFromSource();

            if (self._sourceError) {
              state.detached = true;
              self._consumers.delete(state);
              throw self._sourceError;
            }

            const newBufferIndex = state.cursor - self._bufferStart;
            if (newBufferIndex < self._buffer.length) {
              const chunk = self._buffer.get(newBufferIndex);
              state.cursor++;
              self._tryTrimBuffer();
              return { __proto__: null, done: false, value: chunk };
            }

            if (self._sourceExhausted) {
              state.detached = true;
              self._consumers.delete(state);
              return { __proto__: null, done: true, value: undefined };
            }

            return { __proto__: null, done: true, value: undefined };
          },

          async return() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            self._consumers.delete(state);
            self._tryTrimBuffer();
            return { __proto__: null, done: true, value: undefined };
          },

          async throw() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            self._consumers.delete(state);
            self._tryTrimBuffer();
            return { __proto__: null, done: true, value: undefined };
          },
        };
      },
    };
  }

  cancel(reason) {
    if (this._cancelled) return;
    this._cancelled = true;

    if (reason) {
      this._sourceError = reason;
    }

    if (this._sourceIterator?.return) {
      this._sourceIterator.return().catch(() => {});
    }

    for (const consumer of this._consumers) {
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
    this._consumers.clear();

    for (let i = 0; i < this._pullWaiters.length; i++) {
      this._pullWaiters[i]();
    }
    this._pullWaiters = [];
  }

  [SymbolDispose]() {
    this.cancel();
  }

  // Internal methods

  async _waitForBufferSpace(_state) {
    while (this._buffer.length >= this._options.highWaterMark) {
      if (this._cancelled || this._sourceError || this._sourceExhausted) {
        return !this._cancelled;
      }

      switch (this._options.backpressure) {
        case 'strict':
          throw new ERR_OUT_OF_RANGE(
            'buffer size', `<= ${this._options.highWaterMark}`,
            this._buffer.length);
        case 'block':
          await new Promise((resolve) => {
            ArrayPrototypePush(this._pullWaiters, resolve);
          });
          break;
        case 'drop-oldest':
          this._buffer.shift();
          this._bufferStart++;
          for (const consumer of this._consumers) {
            if (consumer.cursor < this._bufferStart) {
              consumer.cursor = this._bufferStart;
            }
          }
          return true;
        case 'drop-newest':
          return true;
      }
    }
    return true;
  }

  _pullFromSource() {
    if (this._sourceExhausted || this._cancelled) {
      return PromiseResolve();
    }

    if (this._pulling) {
      return new Promise((resolve) => {
        ArrayPrototypePush(this._pullWaiters, resolve);
      });
    }

    this._pulling = true;

    return (async () => {
      try {
        if (!this._sourceIterator) {
          if (isAsyncIterable(this._source)) {
            this._sourceIterator =
              this._source[SymbolAsyncIterator]();
          } else if (isSyncIterable(this._source)) {
            const syncIterator =
              this._source[SymbolIterator]();
            this._sourceIterator = {
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
              'source', ['AsyncIterable', 'Iterable'], this._source);
          }
        }

        const result = await this._sourceIterator.next();

        if (result.done) {
          this._sourceExhausted = true;
        } else {
          this._buffer.push(result.value);
        }
      } catch (error) {
        this._sourceError =
          error instanceof Error ? error : new ERR_OPERATION_FAILED(String(error));
        this._sourceExhausted = true;
      } finally {
        this._pulling = false;
        for (let i = 0; i < this._pullWaiters.length; i++) {
          this._pullWaiters[i]();
        }
        this._pullWaiters = [];
      }
    })();
  }

  _getMinCursor() {
    let min = Infinity;
    for (const consumer of this._consumers) {
      if (consumer.cursor < min) {
        min = consumer.cursor;
      }
    }
    return min === Infinity ?
      this._bufferStart + this._buffer.length : min;
  }

  _tryTrimBuffer() {
    const minCursor = this._getMinCursor();
    const trimCount = minCursor - this._bufferStart;
    if (trimCount > 0) {
      this._buffer.trimFront(trimCount);
      this._bufferStart = minCursor;
      for (let i = 0; i < this._pullWaiters.length; i++) {
        this._pullWaiters[i]();
      }
      this._pullWaiters = [];
    }
  }
}

// =============================================================================
// Sync Share Implementation
// =============================================================================

class SyncShareImpl {
  constructor(source, options) {
    this._source = source;
    this._options = options;
    this._buffer = new RingBuffer();
    this._bufferStart = 0;
    this._consumers = new SafeSet();
    this._sourceIterator = null;
    this._sourceExhausted = false;
    this._sourceError = null;
    this._cancelled = false;
  }

  get consumerCount() {
    return this._consumers.size;
  }

  get bufferSize() {
    return this._buffer.length;
  }

  pull(...transforms) {
    const rawConsumer = this._createRawConsumer();

    if (transforms.length > 0) {
      return pullSyncWithTransforms(rawConsumer, ...transforms);
    }
    return rawConsumer;
  }

  _createRawConsumer() {
    const state = {
      cursor: this._bufferStart,
      detached: false,
    };

    this._consumers.add(state);
    const self = this;

    return {
      [SymbolIterator]() {
        return {
          next() {
            if (state.detached) {
              return { done: true, value: undefined };
            }
            if (self._sourceError) {
              state.detached = true;
              self._consumers.delete(state);
              throw self._sourceError;
            }
            if (self._cancelled) {
              state.detached = true;
              self._consumers.delete(state);
              return { done: true, value: undefined };
            }

            const bufferIndex = state.cursor - self._bufferStart;
            if (bufferIndex < self._buffer.length) {
              const chunk = self._buffer.get(bufferIndex);
              state.cursor++;
              self._tryTrimBuffer();
              return { done: false, value: chunk };
            }

            if (self._sourceExhausted) {
              state.detached = true;
              self._consumers.delete(state);
              return { done: true, value: undefined };
            }

            // Check buffer limit
            if (self._buffer.length >= self._options.highWaterMark) {
              switch (self._options.backpressure) {
                case 'strict':
                  throw new ERR_OUT_OF_RANGE(
                    'buffer size', `<= ${self._options.highWaterMark}`,
                    self._buffer.length);
                case 'block':
                  throw new ERR_OUT_OF_RANGE(
                    'buffer size', `<= ${self._options.highWaterMark} ` +
                    '(blocking not available in sync context)',
                    self._buffer.length);
                case 'drop-oldest':
                  self._buffer.shift();
                  self._bufferStart++;
                  for (const consumer of self._consumers) {
                    if (consumer.cursor < self._bufferStart) {
                      consumer.cursor = self._bufferStart;
                    }
                  }
                  break;
                case 'drop-newest':
                  state.detached = true;
                  self._consumers.delete(state);
                  return { done: true, value: undefined };
              }
            }

            self._pullFromSource();

            if (self._sourceError) {
              state.detached = true;
              self._consumers.delete(state);
              throw self._sourceError;
            }

            const newBufferIndex = state.cursor - self._bufferStart;
            if (newBufferIndex < self._buffer.length) {
              const chunk = self._buffer.get(newBufferIndex);
              state.cursor++;
              self._tryTrimBuffer();
              return { done: false, value: chunk };
            }

            if (self._sourceExhausted) {
              state.detached = true;
              self._consumers.delete(state);
              return { done: true, value: undefined };
            }

            return { done: true, value: undefined };
          },

          return() {
            state.detached = true;
            self._consumers.delete(state);
            self._tryTrimBuffer();
            return { done: true, value: undefined };
          },

          throw() {
            state.detached = true;
            self._consumers.delete(state);
            self._tryTrimBuffer();
            return { done: true, value: undefined };
          },
        };
      },
    };
  }

  cancel(reason) {
    if (this._cancelled) return;
    this._cancelled = true;

    if (reason) {
      this._sourceError = reason;
    }

    if (this._sourceIterator?.return) {
      this._sourceIterator.return();
    }

    for (const consumer of this._consumers) {
      consumer.detached = true;
    }
    this._consumers.clear();
  }

  [SymbolDispose]() {
    this.cancel();
  }

  _pullFromSource() {
    if (this._sourceExhausted || this._cancelled) return;

    try {
      this._sourceIterator ||= this._source[SymbolIterator]();

      const result = this._sourceIterator.next();

      if (result.done) {
        this._sourceExhausted = true;
      } else {
        this._buffer.push(result.value);
      }
    } catch (error) {
      this._sourceError =
        error instanceof Error ? error : new ERR_OPERATION_FAILED(String(error));
      this._sourceExhausted = true;
    }
  }

  _getMinCursor() {
    let min = Infinity;
    for (const consumer of this._consumers) {
      if (consumer.cursor < min) {
        min = consumer.cursor;
      }
    }
    return min === Infinity ?
      this._bufferStart + this._buffer.length : min;
  }

  _tryTrimBuffer() {
    const minCursor = this._getMinCursor();
    const trimCount = minCursor - this._bufferStart;
    if (trimCount > 0) {
      this._buffer.trimFront(trimCount);
      this._bufferStart = minCursor;
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
