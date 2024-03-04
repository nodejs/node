'use strict';

const {
  ArrayPrototypePushApply,
  AsyncIteratorPrototype,
  ObjectSetPrototypeOf,
  Promise,
  PromiseResolve,
  SymbolAsyncIterator,
  SymbolIterator,
} = primordials;

const {
  codes: { ERR_INVALID_ARG_TYPE },
} = require('internal/errors');
const FixedQueue = require('internal/fixed_queue');

/**
 * Wraps an iterable in a debounced iterable. When trying to get the next item,
 * the debounced iterable will group all items that are returned less than
 * `delay` milliseconds apart into a single batch.
 *
 * The debounced iterable will only start consuming the original iterable when
 * the consumer requests a result (through `next` calls), and will stop
 * consuming the original iterable when no more items are requested.
 *
 * Each debounced iterable item will be an array of items from the original
 * iterable, and will always contain at least one item. This allows the consumer
 * to decide how to handle the batch of items (e.g. take the latest only, filter
 * duplicates, etc.).
 *
 * @template T
 * @param {Iterable<T> | AsyncIterable<T>} iterable
 * @param {number} delay
 * @returns {AsyncIterableIterator<[T, ...T[]]>}
 */
exports.debounceIterable = function debounceIterable(iterable, delay) {
  const innerIterator =
    SymbolAsyncIterator in iterable
      ? iterable[SymbolAsyncIterator]()
      : iterable[SymbolIterator]();

  let doneProducing = false;
  let doneConsuming = false;
  let consuming = false;
  let error = null;
  let timer = null;

  const unconsumedPromises = new FixedQueue();
  let unconsumedValues = [];

  return ObjectSetPrototypeOf(
    {
      [SymbolAsyncIterator]() {
        return this;
      },

      next() {
        return new Promise((resolve, reject) => {
          unconsumedPromises.push({ resolve, reject });
          startConsuming();
        });
      },

      return() {
        return closeHandler();
      },

      throw(err) {
        if (!err || !(err instanceof Error)) {
          throw new ERR_INVALID_ARG_TYPE('AsyncIterator.throw', 'Error', err);
        }
        errorHandler(err);
      },
    },
    AsyncIteratorPrototype
  );

  async function startConsuming() {
    if (consuming) return;

    consuming = true;

    while (!doneProducing && !doneConsuming && !unconsumedPromises.isEmpty()) {
      try {
        // if `result` takes longer than `delay` to resolve, make sure any
        // unconsumedValue are flushed.
        scheduleFlush();

        const result = await innerIterator.next();

        // A new value just arrived. Make sure we wont flush just yet.
        unscheduleFlush();

        if (result.done) {
          doneProducing = true;
        } else if (!doneConsuming) {
          ArrayPrototypePushApply(unconsumedValues, result.value);
        }
      } catch (err) {
        doneProducing = true;
        error ||= err;
      }
    }

    flushNow();

    consuming = false;
  }

  function scheduleFlush() {
    if (timer == null) {
      timer = setTimeout(flushNow, delay).unref();
    }
  }

  function unscheduleFlush() {
    if (timer != null) {
      clearTimeout(timer);
      timer = null;
    }
  }

  function flushNow() {
    unscheduleFlush();

    if (!doneConsuming) {
      if (unconsumedValues.length > 0 && !unconsumedPromises.isEmpty()) {
        unconsumedPromises
          .shift()
          .resolve({ done: false, value: unconsumedValues });
        unconsumedValues = [];
      }
      if (doneProducing && unconsumedValues.length === 0) {
        doneConsuming = true;
      }
    }

    while (doneConsuming && !unconsumedPromises.isEmpty()) {
      const { resolve, reject } = unconsumedPromises.shift();
      if (error) reject(error);
      else resolve({ done: true, value: undefined });
    }
  }

  function errorHandler(err) {
    error ||= err;

    closeHandler();
  }

  function closeHandler() {
    doneConsuming = true;
    unconsumedValues = [];

    flushNow();

    if (!doneProducing) {
      doneProducing = true;
      innerIterator.return?.();
    }

    return PromiseResolve({ done: true, value: undefined });
  }
};
