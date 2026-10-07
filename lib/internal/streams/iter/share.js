'use strict';

// New Streams API - Share
//
// Pull-model multi-consumer streaming. Shares a single source among
// multiple consumers with explicit buffering.

const {
  ArrayPrototypePush,
  FunctionPrototypeCall,
  ObjectSetPrototypeOf,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  SafeSet,
  Symbol,
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
  pullSync: pullSyncWithTransforms,
  pullWithConsumerCleanup,
} = require('internal/streams/iter/pull');

const {
  IterResult,
  kMultiConsumerDefaultBudget,
  createBatchEntry,
  getProtocolMethod,
  getMinCursor,
  onSignalAbort,
  parsePullArgs,
  splitBatchEntry,
  validateBatchEntry,
  validateBudget,
} = require('internal/streams/iter/utils');
const {
  converters,
} = require('internal/streams/iter/webidl');

const {
  RingBuffer,
} = require('internal/streams/iter/ringbuffer');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_RETURN_VALUE,
    ERR_INVALID_STATE,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const { markPromiseAsHandled } = internalBinding('util');

// =============================================================================
// Async Share Implementation
// =============================================================================

const kNoShareError = Symbol('kNoShareError');
// Returned by #tryRead() when the consumer must wait for the source.
const kPull = Symbol('kPull');
const kSetFactorySignal = Symbol('kSetFactorySignal');

class ShareImpl {
  #source;
  #options;
  #buffer = new RingBuffer();
  #bufferStart = 0;
  #consumers = new SafeSet();
  #sourceIterator = null;
  #sourceExhausted = false;
  #sourceError = kNoShareError;
  #cancelled = false;
  #pulling = false;
  #pullWaiters = [];
  // While the source is read: the promise that every consumer waiting for
  // the read waits for, the function resolving it, and whether the batch
  // read is to be discarded ('drop-newest'). A cancellation resolves it at
  // once: racing the read with a promise settled by cancel() would add
  // reactions to that promise on every read, which would be kept until the
  // share is cancelled or collected.
  #pullDone = null;
  #resolvePull = null;
  #pullDiscard = false;
  #cancelError = kNoShareError;
  #cachedMinCursor = 0;
  #cachedMinCursorConsumers = 0;
  #abortHandler;
  /** Cumulative byte size of buffered entries */
  #bufferedBytes = 0;

  constructor(source, options) {
    this.#source = source;
    this.#options = options;
  }

  get consumerCount() {
    return this.#consumers.size;
  }

  [kSetFactorySignal](signal) {
    this.#abortHandler = () => this.cancel(signal.reason);
    onSignalAbort(signal, this.#abortHandler);
  }

  pull(...args) {
    const parsed = parsePullArgs(args);
    const { transforms } = parsed;
    const options = converters.PullOptions(parsed.options, {
      __proto__: null,
      context: 'options',
    });
    const { signal } = options;

    // Avoid registering a consumer that the pre-aborted pipeline will never
    // read or detach.
    if (signal?.aborted) {
      return {
        __proto__: null,
        // eslint-disable-next-line require-yield
        async *[SymbolAsyncIterator]() {
          throw signal.reason;
        },
      };
    }

    const rawConsumer = this.#createRawConsumer();

    if (transforms.length > 0 || signal) {
      return pullWithConsumerCleanup(rawConsumer, transforms, signal);
    }
    return rawConsumer;
  }

  #createRawConsumer() {
    const state = ObjectSetPrototypeOf({
      cursor: this.#bufferStart,
      resolve: null,
      reject: null,
      detached: false,
      error: kNoShareError,
      // Set by #readAsync() for next(): the read waits for the source.
      waiting: false,
      // Created by #readAfterPull() on first use.
      afterPull: undefined,
      // The last pending next() of the consumer, if it is waiting for the
      // source: the next one waits for it, as next() calls of an async
      // generator do.
      pendingNext: null,
    }, null);

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
        // A read that finds data, or the end, settles at once; one that
        // waits for the source becomes pendingNext.
        const read = () => {
          let result;
          try {
            result = self.#tryRead(state);
          } catch (error) {
            return PromiseReject(error);
          }
          if (result !== kPull) return PromiseResolve(result);
          return self.#readAsync(state);
        };
        const clearPending = (pending) => {
          const onSettled = () => {
            if (state.pendingNext === pending) state.pendingNext = null;
          };
          PromisePrototypeThen(pending, onSettled, onSettled);
        };

        return ObjectSetPrototypeOf({
          next() {
            let next;
            if (state.pendingNext !== null) {
              next = PromisePrototypeThen(state.pendingNext, read, read);
            } else {
              state.waiting = false;
              next = read();
              // Settled at once: nothing for a later next() to wait for.
              if (!state.waiting) return next;
            }
            state.pendingNext = next;
            markPromiseAsHandled(next);
            clearPending(next);
            return next;
          },

          async return() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            if (self.#deleteConsumer(state)) {
              self.#tryTrimBuffer();
            }
            return new IterResult(true, undefined);
          },

          async throw() {
            state.detached = true;
            state.resolve = null;
            state.reject = null;
            if (self.#deleteConsumer(state)) {
              self.#tryTrimBuffer();
            }
            return new IterResult(true, undefined);
          },
        }, null);
      },
    };
  }

  cancel(reason) {
    if (this.#cancelled) return;
    const hasReason = arguments.length > 0;
    this.#cancelled = true;

    if (hasReason) {
      this.#cancelError = reason;
    }

    if (this.#resolvePull !== null) this.#finishPull();

    try {
      const returnMethod = this.#sourceIterator?.return;
      if (typeof returnMethod === 'function') {
        const cleanup = PromisePrototypeThen(
          PromiseResolve(FunctionPrototypeCall(
            returnMethod, this.#sourceIterator)),
          undefined,
          undefined);
        markPromiseAsHandled(cleanup);
      }
    } catch {
      // Cancellation has precedence over source cleanup errors.
    }

    for (const consumer of this.#consumers) {
      consumer.error = this.#cancelError;
      if (consumer.resolve) {
        if (hasReason) {
          consumer.reject?.(reason);
        } else {
          consumer.resolve(new IterResult(true, undefined));
        }
        consumer.resolve = null;
        consumer.reject = null;
      }
      consumer.detached = true;
    }
    this.#consumers.clear();
    this.#buffer.clear();
    this.#bufferedBytes = 0;
    this.#cleanupFactorySignal();

    for (let i = 0; i < this.#pullWaiters.length; i++) {
      this.#pullWaiters[i]();
    }
    this.#pullWaiters = [];
  }

  [SymbolDispose]() {
    this.cancel();
  }

  // Internal methods

  // Read the consumer's next batch if it is buffered, or the end: returns
  // its result, or kPull if the source must be read first.
  #tryRead(state) {
    if (state.detached) {
      if (state.error !== kNoShareError) throw state.error;
      return new IterResult(true, undefined);
    }

    if (this.#cancelled) {
      state.detached = true;
      state.error = this.#cancelError;
      this.#deleteConsumer(state);
      if (state.error !== kNoShareError) throw state.error;
      return new IterResult(true, undefined);
    }

    // Check if data is available in buffer
    const bufferIndex = state.cursor - this.#bufferStart;
    if (bufferIndex < this.#buffer.length) {
      const chunk = this.#readEntry(this.#buffer.get(bufferIndex));
      const cursor = state.cursor;
      state.cursor++;
      if (cursor === this.#cachedMinCursor &&
          --this.#cachedMinCursorConsumers === 0) {
        this.#tryTrimBuffer();
      }
      return new IterResult(false, chunk);
    }

    if (this.#sourceExhausted) {
      state.detached = true;
      this.#deleteConsumer(state);
      if (this.#sourceError !== kNoShareError) {
        state.error = this.#sourceError;
        throw state.error;
      }
      return new IterResult(true, undefined);
    }
    return kPull;
  }

  // Read the consumer's next batch once the source has been read. Loops
  // until it gets data, the source is exhausted, or the consumer is
  // detached: multiple consumers may be woken after a single pull, and
  // those that find no data at their cursor must pull again rather than
  // terminate prematurely.
  #readAsync(state) {
    state.waiting = true;
    if (this.#bufferedBytes < this.#options.budget) {
      return this.#readAfterPull(state);
    }
    return this.#readAsyncLoop(state);
  }

  async #readAsyncLoop(state) {
    for (;;) {
      // Need to pull from source - check buffer limit
      let shouldBuffer;
      if (this.#bufferedBytes < this.#options.budget) {
        shouldBuffer = true;
      } else {
        try {
          shouldBuffer = await this.#waitForBufferSpace();
        } catch (error) {
          state.detached = true;
          if (this.#deleteConsumer(state)) {
            this.#tryTrimBuffer();
          }
          throw error;
        }
      }
      if (shouldBuffer === null) {
        state.detached = true;
        state.error = this.#cancelError;
        this.#deleteConsumer(state);
        if (state.error !== kNoShareError) throw state.error;
        return new IterResult(true, undefined);
      }

      await this.#pullFromSource(!shouldBuffer);
      if (!shouldBuffer) {
        await this.#waitForBufferSpaceAfterDrop();
      }

      const result = this.#tryRead(state);
      if (result !== kPull) return result;
    }
  }

  // The common case of #readAsyncLoop(), without an async function: there
  // is room in the buffer, so read the source and then the consumer's batch.
  #readAfterPull(state) {
    state.afterPull ??= () => {
      const result = this.#tryRead(state);
      return result !== kPull ? result : this.#readAsyncLoop(state);
    };
    return PromisePrototypeThen(this.#pullFromSource(false), state.afterPull);
  }

  async #waitForBufferSpace() {
    while (this.#bufferedBytes >= this.#options.budget) {
      if (this.#cancelled ||
          this.#sourceError !== kNoShareError ||
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
            this.#bufferedBytes -= evicted.byteLength;
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

  async #waitForBufferSpaceAfterDrop() {
    while (this.#bufferedBytes >= this.#options.budget &&
           !this.#cancelled &&
           this.#sourceError === kNoShareError &&
           !this.#sourceExhausted) {
      const { promise, resolve } = PromiseWithResolvers();
      ArrayPrototypePush(this.#pullWaiters, resolve);
      await promise;
    }
  }

  #pullFromSource(discard = false) {
    if (this.#sourceExhausted || this.#cancelled) {
      return PromiseResolve();
    }

    if (this.#pulling) return this.#pullDone;

    this.#pulling = true;
    const { promise, resolve } = PromiseWithResolvers();
    this.#pullDone = promise;
    this.#resolvePull = resolve;
    this.#pullDiscard = discard;

    let next;
    try {
      if (!this.#sourceIterator) {
        if (isAsyncIterable(this.#source)) {
          this.#sourceIterator =
            this.#source[SymbolAsyncIterator]();
        } else if (isSyncIterable(this.#source)) {
          const syncIterator =
            this.#source[SymbolIterator]();
          this.#sourceIterator = ObjectSetPrototypeOf({
            // Its result is passed to PromiseResolve().
            next() {
              return syncIterator.next();
            },
            async return() {
              return syncIterator.return?.() ??
                     new IterResult(true, undefined);
            },
          }, null);
        } else {
          throw new ERR_INVALID_ARG_TYPE(
            'source', ['AsyncIterable', 'Iterable'], this.#source);
        }
      }
      next = this.#sourceIterator.next();
    } catch (error) {
      this.#onReadError(error);
      return promise;
    }
    PromisePrototypeThen(PromiseResolve(next), this.#onReadResult,
                         this.#onReadError);
    return promise;
  }

  // The handlers of a read of the source, created once per share. A read
  // ended by a cancellation is ignored.
  #onReadResult = (result) => {
    if (this.#resolvePull === null || this.#cancelled) return;
    try {
      if (result.done) {
        this.#sourceExhausted = true;
      } else if (!this.#pullDiscard) {
        this.#bufferBatch(result.value);
      }
    } catch (error) {
      this.#sourceError = error;
      this.#sourceExhausted = true;
    }
    this.#finishPull();
  };

  #onReadError = (error) => {
    if (this.#resolvePull === null || this.#cancelled) return;
    this.#sourceError = error;
    this.#sourceExhausted = true;
    this.#finishPull();
  };

  #finishPull() {
    const resolve = this.#resolvePull;
    this.#resolvePull = null;
    this.#pullDone = null;
    if (this.#sourceExhausted && this.#consumers.size === 0) {
      this.#cleanupFactorySignal();
    }
    this.#pulling = false;
    for (let i = 0; i < this.#pullWaiters.length; i++) {
      this.#pullWaiters[i]();
    }
    this.#pullWaiters = [];
    resolve();
  }

  #bufferBatch(batch) {
    const entry = createBatchEntry(batch);
    // 'drop-oldest' evicts whole entries. A single pulled batch can be much
    // larger than the budget (for example when from() combines many values
    // of a sync source), and evicting it would discard every chunk in it,
    // including ones that no consumer has read. Split such batches so that
    // eviction keeps the newest chunks that fit the budget.
    const entries = this.#options.backpressure === 'drop-oldest' ?
      splitBatchEntry(entry, this.#options.budget) : undefined;
    if (entries === undefined) {
      this.#buffer.push(entry);
    } else {
      for (let i = 0; i < entries.length; i++) {
        this.#buffer.push(entries[i]);
      }
    }
    this.#bufferedBytes += entry.byteLength;
  }

  #tryTrimBuffer() {
    // Retain buffered data for consumers that attach while none are active.
    // Without this, the last consumer detaching would discard entries that
    // a late-joining consumer has not read and cannot get back from the
    // source.
    if (this.#consumers.size === 0) return;
    if (this.#cachedMinCursorConsumers === 0) {
      this.#recomputeMinCursor();
    }
    const trimCount = this.#cachedMinCursor - this.#bufferStart;
    if (trimCount > 0) {
      for (let i = 0; i < trimCount; i++) {
        const evicted = this.#buffer.get(i);
        this.#bufferedBytes -= evicted.byteLength;
      }
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = this.#cachedMinCursor;
      for (let i = 0; i < this.#pullWaiters.length; i++) {
        this.#pullWaiters[i]();
      }
      this.#pullWaiters = [];
    }
  }

  #readEntry(entry) {
    try {
      return validateBatchEntry(entry);
    } catch (error) {
      this.cancel(error);
      this.#buffer.clear();
      this.#bufferedBytes = 0;
      throw error;
    }
  }

  #recomputeMinCursor() {
    const { minCursor, minCursorConsumers } = getMinCursor(
      this.#consumers, this.#bufferStart + this.#buffer.length);
    this.#cachedMinCursor = minCursor;
    this.#cachedMinCursorConsumers = minCursorConsumers;
  }

  #cleanupFactorySignal() {
    if (this.#abortHandler !== undefined) {
      this.#options.signal.removeEventListener('abort', this.#abortHandler);
      this.#abortHandler = undefined;
    }
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
      const shouldTrimBuffer = this.#deleteConsumerFromMin(consumer);
      if (this.#sourceExhausted && this.#consumers.size === 0) {
        this.#cleanupFactorySignal();
      }
      return shouldTrimBuffer;
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
  #sourceError = kNoShareError;
  #cancelled = false;
  #pulling = false;
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
    const state = ObjectSetPrototypeOf({
      cursor: this.#bufferStart,
      detached: false,
      error: kNoShareError,
    }, null);

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
        return ObjectSetPrototypeOf({
          next() {
            if (state.detached) {
              if (state.error !== kNoShareError) throw state.error;
              return new IterResult(true, undefined);
            }
            if (self.#sourceError !== kNoShareError) {
              state.detached = true;
              state.error = self.#sourceError;
              self.#deleteConsumer(state);
              throw state.error;
            }
            if (self.#cancelled) {
              state.detached = true;
              self.#deleteConsumer(state);
              return new IterResult(true, undefined);
            }

            const bufferIndex = state.cursor - self.#bufferStart;
            if (bufferIndex < self.#buffer.length) {
              const chunk = self.#readEntry(self.#buffer.get(bufferIndex));
              const cursor = state.cursor;
              state.cursor++;
              if (cursor === self.#cachedMinCursor &&
                  --self.#cachedMinCursorConsumers === 0) {
                self.#tryTrimBuffer();
              }
              return new IterResult(false, chunk);
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#deleteConsumer(state);
              return new IterResult(true, undefined);
            }

            // Check buffer limit. 'unbounded' and 'drop-newest' are rejected
            // by shareSync().
            if (self.#bufferedBytes >= self.#options.budget) {
              switch (self.#options.backpressure) {
                case 'strict': {
                  const error = new ERR_OUT_OF_RANGE(
                    'buffered bytes', `< ${self.#options.budget}`,
                    self.#bufferedBytes);
                  // Detach before throwing, as the async share does. Neither
                  // for...of nor a transform pipeline calls return() when
                  // next() throws, so a consumer left registered here would
                  // keep its cursor forever and wedge the other consumers.
                  state.detached = true;
                  if (self.#deleteConsumer(state)) {
                    self.#tryTrimBuffer();
                  }
                  throw error;
                }
                case 'drop-oldest':
                  while (self.#bufferedBytes >= self.#options.budget &&
                         self.#buffer.length > 0) {
                    const evicted = self.#buffer.shift();
                    self.#bufferedBytes -= evicted.byteLength;
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
              }
            }

            self.#pullFromSource();

            if (self.#sourceError !== kNoShareError) {
              state.detached = true;
              state.error = self.#sourceError;
              self.#deleteConsumer(state);
              throw state.error;
            }

            const newBufferIndex = state.cursor - self.#bufferStart;
            if (newBufferIndex < self.#buffer.length) {
              const chunk = self.#readEntry(self.#buffer.get(newBufferIndex));
              const cursor = state.cursor;
              state.cursor++;
              if (cursor === self.#cachedMinCursor &&
                  --self.#cachedMinCursorConsumers === 0) {
                self.#tryTrimBuffer();
              }
              return new IterResult(false, chunk);
            }

            if (self.#sourceExhausted) {
              state.detached = true;
              self.#deleteConsumer(state);
              return new IterResult(true, undefined);
            }

            return new IterResult(true, undefined);
          },

          return() {
            state.detached = true;
            if (self.#deleteConsumer(state)) {
              self.#tryTrimBuffer();
            }
            return new IterResult(true, undefined);
          },

          throw() {
            state.detached = true;
            if (self.#deleteConsumer(state)) {
              self.#tryTrimBuffer();
            }
            return new IterResult(true, undefined);
          },
        }, null);
      },
    };
  }

  cancel(reason) {
    if (this.#cancelled) return;
    const hasReason = arguments.length > 0;
    this.#cancelled = true;

    if (hasReason) {
      this.#sourceError = reason;
    }

    try {
      const returnMethod = this.#sourceIterator?.return;
      if (typeof returnMethod === 'function') {
        FunctionPrototypeCall(returnMethod, this.#sourceIterator);
      }
    } catch {
      // Cancellation has precedence over source cleanup errors.
    }

    for (const consumer of this.#consumers) {
      if (hasReason) consumer.error = reason;
      consumer.detached = true;
    }
    this.#consumers.clear();
  }

  [SymbolDispose]() {
    this.cancel();
  }

  #pullFromSource() {
    if (this.#sourceExhausted || this.#cancelled) return;

    // A source that reads from its own share would re-enter its own
    // iterator. Fail that read without touching the share's state; the
    // source sees the error and may handle it.
    if (this.#pulling) {
      throw new ERR_INVALID_STATE(
        'shareSync() source cannot be read while it is producing a value');
    }

    this.#pulling = true;
    try {
      this.#sourceIterator ||= this.#source[SymbolIterator]();

      const result = this.#sourceIterator.next();

      if (result.done) {
        this.#sourceExhausted = true;
      } else {
        this.#bufferBatch(result.value);
      }
    } catch (error) {
      this.#sourceError = error;
      this.#sourceExhausted = true;
    } finally {
      this.#pulling = false;
    }
  }

  #bufferBatch(batch) {
    const entry = createBatchEntry(batch);
    // 'drop-oldest' evicts whole entries. A single pulled batch can be much
    // larger than the budget (for example when from() combines many values
    // of a sync source), and evicting it would discard every chunk in it,
    // including ones that no consumer has read. Split such batches so that
    // eviction keeps the newest chunks that fit the budget.
    const entries = this.#options.backpressure === 'drop-oldest' ?
      splitBatchEntry(entry, this.#options.budget) : undefined;
    if (entries === undefined) {
      this.#buffer.push(entry);
    } else {
      for (let i = 0; i < entries.length; i++) {
        this.#buffer.push(entries[i]);
      }
    }
    this.#bufferedBytes += entry.byteLength;
  }

  #tryTrimBuffer() {
    // Retain buffered data for consumers that attach while none are active.
    // Without this, the last consumer detaching would discard entries that
    // a late-joining consumer has not read and cannot get back from the
    // source.
    if (this.#consumers.size === 0) return;
    if (this.#cachedMinCursorConsumers === 0) {
      this.#recomputeMinCursor();
    }
    const trimCount = this.#cachedMinCursor - this.#bufferStart;
    if (trimCount > 0) {
      for (let i = 0; i < trimCount; i++) {
        const evicted = this.#buffer.get(i);
        this.#bufferedBytes -= evicted.byteLength;
      }
      this.#buffer.trimFront(trimCount);
      this.#bufferStart = this.#cachedMinCursor;
    }
  }

  #readEntry(entry) {
    try {
      return validateBatchEntry(entry);
    } catch (error) {
      this.cancel(error);
      this.#buffer.clear();
      this.#bufferedBytes = 0;
      throw error;
    }
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
// Public API
// =============================================================================

function share(source, options = { __proto__: null }) {
  // Normalize source via from() - accepts strings, ArrayBuffers, protocols, etc.
  const normalized = from(source);
  options = converters.ShareOptions(options, {
    __proto__: null,
    context: 'options',
  });
  const {
    budget = kMultiConsumerDefaultBudget,
    backpressure = 'strict',
    signal,
  } = options;
  validateBudget(budget);

  const opts = {
    __proto__: null,
    budget,
    backpressure,
    signal,
  };

  const shareImpl = new ShareImpl(normalized, opts);

  if (signal) {
    shareImpl[kSetFactorySignal](signal);
  }

  return shareImpl;
}

function shareSync(source, options = { __proto__: null }) {
  // Normalize source via fromSync() - accepts strings, ArrayBuffers, protocols, etc.
  const normalized = fromSync(source);
  options = converters.ShareSyncOptions(options, {
    __proto__: null,
    context: 'options',
  });
  const {
    budget = kMultiConsumerDefaultBudget,
    backpressure = 'strict',
  } = options;
  validateBudget(budget);
  // A synchronous consumer can neither wait for the slowest consumer to
  // release budget ('unbounded') nor keep pulling and discarding until it
  // does ('drop-newest'): the slowest consumer cannot advance while the
  // call is running.
  if (backpressure === 'unbounded' || backpressure === 'drop-newest') {
    throw new ERR_INVALID_ARG_VALUE(
      'options.backpressure', backpressure,
      `${backpressure} is not supported by shareSync()`);
  }

  const opts = {
    __proto__: null,
    budget,
    backpressure,
  };

  return new SyncShareImpl(normalized, opts);
}

const Share = {
  __proto__: null,
  from(input, options) {
    const protocol = getProtocolMethod(input, shareProtocol);
    if (protocol !== undefined) {
      const result = FunctionPrototypeCall(protocol, input, options);
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
    const protocol = getProtocolMethod(input, shareSyncProtocol);
    if (protocol !== undefined) {
      const result = FunctionPrototypeCall(protocol, input, options);
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
