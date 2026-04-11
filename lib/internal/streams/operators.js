'use strict';

const {
  ArrayPrototypePush,
  Boolean,
  FunctionPrototypeCall,
  MathFloor,
  Number,
  NumberIsNaN,
  Promise,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  PromiseWithResolvers,
  Symbol,
} = primordials;

const { AbortController, AbortSignal } = require('internal/abort_controller');

const {
  AbortError,
  aggregateTwoErrors,
  codes: {
    ERR_MISSING_ARGS,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');
const {
  validateAbortSignal,
  validateInteger,
  validateObject,
  validateFunction,
  validateBoolean,
} = require('internal/validators');
const { kWeakHandler, kResistStopPropagation } = require('internal/event_target');
const { eos, finished } = require('internal/streams/end-of-stream');
const destroyImpl = require('internal/streams/destroy');

const kEmpty = Symbol('kEmpty');
const kEof = Symbol('kEof');

const {
  isPromise,
} = require('internal/util/types');

const nop = () => {};
function createWaiter() {
  let resolve = nop;

  function reset(r) {
    resolve = r;
  }

  function wait() {
    return new Promise(reset);
  }

  function notify() {
    resolve();
    resolve = nop;
  }

  return { __proto__: null, wait, reset, notify };
}

function map(fn, options) {
  validateFunction(fn, 'fn');
  if (options != null) {
    validateObject(options, 'options');
  }
  if (options?.signal != null) {
    validateAbortSignal(options.signal, 'options.signal');
  }

  const concurrency = MathFloor(options?.concurrency ?? 1);
  const highWaterMark = MathFloor(options?.highWaterMark ?? concurrency - 1);

  validateInteger(concurrency, 'options.concurrency', 1);
  validateInteger(highWaterMark, 'options.highWaterMark', 0);

  return createMappedStream(this, fn, options, concurrency, highWaterMark + concurrency);
}

function createMappedStream(source, fn, options, concurrency, highWaterMark) {
  const signal = AbortSignal.any([options?.signal].filter(Boolean));
  const signalOpt = { signal };
  const queue = [];
  const sourceReadable = createWaiter();
  const queueReadable = createWaiter();
  const sourceCapacity = createWaiter();

  let stopped = false;
  let activeMappers = 0;

  function atCapacity() {
    return activeMappers >= concurrency || queue.length >= highWaterMark;
  }

  function maybeResumeSource() {
    if (!stopped && !atCapacity()) {
      sourceCapacity.notify();
    }
  }

  function mapperFinished() {
    activeMappers--;
    maybeResumeSource();
  }

  function mapperRejected() {
    stopped = true;
    mapperFinished();
    queueReadable.notify();
  }

  function enqueueMappedChunk(chunk) {
    activeMappers++;
    const mapped = fn(chunk, signalOpt);

    if (mapped === kEmpty) {
      mapperFinished();
      return mapped;
    }

    if (typeof mapped.then === 'function') {
      PromisePrototypeThen(PromiseResolve(mapped), mapperFinished, mapperRejected);
      queue.push(mapped);
    } else {
      queue.push({ chunk: mapped, afterItemProcessed: mapperFinished });
    }
    queueReadable.notify();
    return mapped;
  }

  async function pumpSource() {
    let error;
    source.on('readable', sourceReadable.notify);
    const cleanup = eos(source, { writable: false }, (err) => {
      error = err ? aggregateTwoErrors(error, err) : null;
      sourceReadable.notify();
    });

    try {
      while (!stopped) {
        if (signal.aborted) {
          throw new AbortError();
        }

        if (atCapacity()) {
          await sourceCapacity.wait();
          continue;
        }

        const chunk = source.destroyed ? null : source.read();
        if (chunk !== null) {
          const mapped = enqueueMappedChunk(chunk);
          if (mapped === kEof) {
            while (queue.length > 0) {
              await sourceCapacity.wait();
            }
            return;
          }
          continue;
        }

        if (error) {
          throw error;
        }
        if (error === null) {
          queue.push(kEof);
          return;
        }
        await sourceReadable.wait();
      }
    } catch (err) {
      error = aggregateTwoErrors(error, err);
      throw error;
    } finally {
      if (
        (error || options?.destroyOnReturn !== false) &&
        (error === undefined || source._readableState.autoDestroy)
      ) {
        destroyImpl.destroyer(source, null);
      } else {
        source.off('readable', sourceReadable.notify);
        cleanup();
      }
    }
  }

  async function runSourcePump() {
    try {
      await pumpSource();
    } catch (err) {
      const rejected = PromiseReject(err);
      PromisePrototypeThen(rejected, mapperFinished, mapperRejected);
      queue.push(rejected);
    } finally {
      stopped = true;
      queueReadable.notify();
    }
  }

  const readable = new source.constructor({
    objectMode: true,
    highWaterMark,
  });
  let readingQueue = false;

  readable._read = async function _read() {
    if (readingQueue) {
      return;
    }
    readingQueue = true;

    try {
      if (!stopped && queue.length === 0) {
        await queueReadable.wait();
      }

      while (queue.length > 0) {
        if (signal.aborted) {
          throw new AbortError();
        }

        let item = queue[0];
        let syncItemFinished;
        if (typeof item.then === 'function') {
          item = await item;
          queue.shift();
        } else if (typeof item.afterItemProcessed === 'function') {
          syncItemFinished = item.afterItemProcessed;
          item = item.chunk;
          // Keep the item queued until its mapper is marked as finished so
          // async and synchronous items apply the same backpressure.
          syncItemFinished();
          queue.shift();
        } else {
          queue.shift();
        }

        if (item === kEof) {
          stopped = true;
          this.push(null);
          sourceCapacity.notify();
          queueReadable.notify();
          return;
        }

        if (item === kEmpty) {
          maybeResumeSource();
          if (!stopped && queue.length === 0) {
            await queueReadable.wait();
          }
          continue;
        }

        if (syncItemFinished) {
          // Prevent stream.read() from re-entering _read before push clears its
          // internal reading flag.
          await PromiseResolve();
        }
        const needsMore = this.push(item);
        // Let async-iterator cleanup run before pulling another source chunk.
        process.nextTick(maybeResumeSource);
        if (!needsMore) {
          return;
        }
      }
    } catch (err) {
      destroyImpl.destroyer(this, err);
    } finally {
      readingQueue = false;
    }
  };

  readable._destroy = function(err, cb) {
    stopped = true;
    sourceReadable.notify();
    sourceCapacity.notify();
    queueReadable.notify();
    cb(err);
  };

  process.nextTick(runSourcePump);
  return readable;
}

function nowOrLater(fn, fn2, args) {
  const valueOrPromise = fn(...args);
  if (isPromise(valueOrPromise)) {
    return PromisePrototypeThen(valueOrPromise, fn2);
  }
  return fn2(valueOrPromise);
}

async function some(fn, options = undefined) {
  validateFunction(fn, 'fn');
  const someFn = (...args) => {
    return nowOrLater(fn, Boolean, args);
  };
  return (await find.call(this, someFn, options)) !== undefined;
}

async function every(fn, options = undefined) {
  validateFunction(fn, 'fn');
  const everyFn = (...args) => {
    return nowOrLater(fn, (value) => !value, args);
  };
  // https://en.wikipedia.org/wiki/De_Morgan%27s_laws
  return !(await find.call(this, everyFn, options));
}

function find(fn, options) {
  validateFunction(fn, 'fn');

  if (options != null) {
    validateObject(options, 'options');
  }
  const signal = options?.signal;
  if (signal != null) {
    validateAbortSignal(signal, 'options.signal');
  }

  const concurrency = MathFloor(options?.concurrency ?? 1);
  validateInteger(concurrency, 'options.concurrency', 1);

  const destroyOnReturn = options?.destroyOnReturn ?? true;
  validateBoolean(destroyOnReturn, 'options.destroyOnReturn');

  // Concurrent predicates can settle out of order. Stop reading after any
  // match, but keep the lowest index after all active predicates settle.
  const stream = this;
  const { promise, resolve } = PromiseWithResolvers();
  let match;
  let error;
  let activeEvaluations = 0;
  let nextIndex = 0;
  let ended = false;
  let settled = false;
  let draining = false;

  function settle() {
    if (!settled) {
      settled = true;
      resolve();
    }
  }

  function fail(err) {
    if (settled) {
      return;
    }
    error = aggregateTwoErrors(error, err);
    destroyImpl.destroyer(stream, error);
    settle();
  }

  function maybeSettle() {
    if (activeEvaluations === 0 && (match !== undefined || ended)) {
      settle();
    }
  }

  function evaluationFinished(matches, chunk, index) {
    if (matches && (match === undefined || index < match.index)) {
      match = { index, value: chunk };
    }
    activeEvaluations--;
    maybeSettle();

    if (!settled && match === undefined && !draining) {
      onReadable();
    }
  }

  function evaluationRejected(err) {
    activeEvaluations--;
    fail(err);
  }

  function evaluate(chunk, index) {
    activeEvaluations++;

    let matches;
    try {
      matches = fn(chunk, options);
    } catch (err) {
      evaluationRejected(err);
      return;
    }

    if (isPromise(matches)) {
      PromisePrototypeThen(
        matches,
        (result) => evaluationFinished(result, chunk, index),
        evaluationRejected,
      );
    } else {
      evaluationFinished(matches, chunk, index);
    }
  }

  function onReadable() {
    if (draining || settled || match !== undefined) {
      return;
    }

    draining = true;
    try {
      while (
        !settled &&
        match === undefined &&
        activeEvaluations < concurrency
      ) {
        if (signal?.aborted) {
          fail(new AbortError(undefined, { cause: signal.reason }));
          return;
        }

        const chunk = stream.destroyed ? null : stream.read();
        if (chunk === null) {
          return;
        }
        evaluate(chunk, nextIndex++);
      }
    } catch (err) {
      fail(err);
    } finally {
      draining = false;
    }
  }

  function onAbort() {
    fail(new AbortError(undefined, { cause: signal.reason }));
  }

  stream.on('readable', onReadable);

  const cleanup = eos(stream, { writable: false }, (err) => {
    if (settled) {
      return;
    }
    if (err) {
      fail(err);
      return;
    }
    ended = true;
    maybeSettle();
  });

  if (signal != null) {
    signal.addEventListener('abort', onAbort, { once: true });
    if (signal.aborted) {
      onAbort();
    }
  }

  return PromisePrototypeThen(promise, () => {
    stream.off('readable', onReadable);
    signal?.removeEventListener('abort', onAbort);

    if (
      (error || destroyOnReturn !== false) &&
      (error === undefined || stream._readableState.autoDestroy)
    ) {
      destroyImpl.destroyer(stream, error);
    } else {
      cleanup();
    }

    if (error) {
      return PromiseReject(error);
    }
    return match?.value;
  });
}

async function forEach(fn, options) {
  validateFunction(fn, 'fn');
  const forEachFn = (...args) => {
    return nowOrLater(fn, () => false, args);
  };
  await find.call(this, forEachFn, options);
}

function filter(fn, options) {
  validateFunction(fn, 'fn');
  const filterFn = (value, options) => {
    return nowOrLater(fn, (predicate) => (predicate ? value : kEmpty), [value, options]);
  };
  return map.call(this, filterFn, options);
}

// Specific to provide better error to reduce since the argument is only
// missing if the stream has no items in it - but the code is still appropriate
class ReduceAwareErrMissingArgs extends ERR_MISSING_ARGS {
  constructor() {
    super('reduce');
    this.message = 'Reduce of an empty stream requires an initial value';
  }
}

function reduce(reducer, initialValue, options) {
  try {
    return reduceImpl(this, arguments.length > 1, reducer, initialValue, options);
  } catch (err) {
    return PromiseReject(err);
  }
}

function reduceImpl(stream, hasInitialValue, reducer, initialValue, options) {
  validateFunction(reducer, 'reducer');
  if (options != null) {
    validateObject(options, 'options');
  }
  const signal = options?.signal;
  if (signal != null) {
    validateAbortSignal(signal, 'options.signal');
  }

  const { promise, resolve } = PromiseWithResolvers();
  const ac = new AbortController();
  const reducerSignal = ac.signal;
  const reducerOptions = { signal: reducerSignal };
  let reducing = false;
  let draining = false;
  let ended = false;
  let settled = false;
  let error;

  function settle() {
    if (!settled) {
      settled = true;
      resolve();
    }
  }

  function fail(err) {
    if (settled) {
      return;
    }
    error = aggregateTwoErrors(error, err);
    destroyImpl.destroyer(stream, error);
    settle();
  }

  function finish() {
    if (reducing || settled) {
      return;
    }
    if (!hasInitialValue) {
      fail(new ReduceAwareErrMissingArgs());
      return;
    }
    settle();
  }

  function reducerFulfilled(value) {
    reducing = false;
    initialValue = value;
    if (settled) {
      return;
    }
    if (ended) {
      finish();
    } else {
      onReadable();
    }
  }

  function reducerRejected(err) {
    reducing = false;
    fail(err);
  }

  function onReadable() {
    if (draining || reducing || settled) {
      return;
    }

    draining = true;
    try {
      while (!reducing && !settled) {
        if (signal?.aborted) {
          fail(new AbortError(undefined, { cause: signal.reason }));
          break;
        }

        const value = stream.destroyed ? null : stream.read();
        if (value === null) {
          break;
        }

        if (!hasInitialValue) {
          initialValue = value;
          hasInitialValue = true;
          continue;
        }

        let result;
        let resultPromise;
        try {
          result = reducer(initialValue, value, reducerOptions);
          const resultType = typeof result;
          if ((resultType !== 'object' || result === null) && resultType !== 'function') {
            initialValue = result;
            continue;
          }

          if (isPromise(result)) {
            resultPromise = result;
          } else {
            const then = result.then;
            if (typeof then !== 'function') {
              initialValue = result;
              continue;
            }
            resultPromise = PromiseResolve({
              then(resolve, reject) {
                FunctionPrototypeCall(then, result, resolve, reject);
              },
            });
          }
        } catch (err) {
          fail(err);
          break;
        }

        reducing = true;
        PromisePrototypeThen(resultPromise, reducerFulfilled, reducerRejected);
      }
    } catch (err) {
      fail(err);
    } finally {
      draining = false;
    }
  }

  function onAbort() {
    ac.abort();
    fail(new AbortError(undefined, { cause: signal.reason }));
  }

  stream.on('readable', onReadable);

  const cleanup = eos(stream, { writable: false }, (err) => {
    if (settled) {
      return;
    }
    if (err) {
      fail(err);
      return;
    }
    ended = true;
    finish();
  });

  if (signal != null) {
    const opts = {
      once: true,
      [kWeakHandler]: stream,
      [kResistStopPropagation]: true,
    };
    signal.addEventListener('abort', onAbort, opts);
    if (signal.aborted) {
      onAbort();
    }
  }

  return PromisePrototypeThen(promise, () => {
    stream.off('readable', onReadable);
    signal?.removeEventListener('abort', onAbort);
    ac.abort();

    if (error) {
      destroyImpl.destroyer(stream, error);
      return PromiseReject(error);
    }

    cleanup();
    return initialValue;
  });
}

async function toArray(options) {
  if (options != null) {
    validateObject(options, 'options');
  }
  const signal = options?.signal;
  if (signal != null) {
    validateAbortSignal(signal, 'options.signal');
  }

  const destroyOnReturn = options?.destroyOnReturn ?? true;
  validateBoolean(destroyOnReturn, 'options.destroyOnReturn');

  const result = [];

  function onReadable() {
    while (true) {
      const chunk = this.destroyed ? null : this.read();
      if (chunk !== null) {
        ArrayPrototypePush(result, chunk);
        continue;
      } else if (signal?.aborted) {
        this.destroy(new AbortError(undefined, { cause: signal.reason }));
      }
      return;
    }
  }

  this.on('readable', onReadable);

  await finished(this, { writable: false, cleanup: destroyOnReturn });
  this.off('readable', onReadable);
  if (destroyOnReturn !== false) {
    destroyImpl.destroyer(this, null);
  }
  return result;
}

function flatMap(fn, options) {
  const values = map.call(this, fn, options);
  return async function* flatMap() {
    for await (const val of values) {
      yield* val;
    }
  }.call(this);
}

function toIntegerOrInfinity(number) {
  // We coerce here to align with the spec
  // https://github.com/tc39/proposal-iterator-helpers/issues/169
  number = Number(number);
  if (NumberIsNaN(number)) {
    return 0;
  }
  if (number < 0) {
    throw new ERR_OUT_OF_RANGE('number', '>= 0', number);
  }
  return number;
}

function drop(number, options = undefined) {
  if (options != null) {
    validateObject(options, 'options');
  }
  if (options?.signal != null) {
    validateAbortSignal(options.signal, 'options.signal');
  }

  number = toIntegerOrInfinity(number);
  return filter.call(this, () => {
    if (number > 0) {
      number--;
      return false;
    }
    return true;
  }, options);
}

function take(number, options = undefined) {
  if (options != null) {
    validateObject(options, 'options');
  }
  if (options?.signal != null) {
    validateAbortSignal(options.signal, 'options.signal');
  }

  number = toIntegerOrInfinity(number);
  return async function* take() {
    if (options?.signal?.aborted) {
      throw new AbortError();
    }
    for await (const val of this) {
      if (options?.signal?.aborted) {
        throw new AbortError();
      }
      if (number-- > 0) {
        yield val;
      }

      // Don't get another item from iterator in case we reached the end
      if (number <= 0) {
        return;
      }
    }
  }.call(this);
}

module.exports.streamReturningOperators = {
  drop,
  filter,
  flatMap,
  map,
  take,
};

module.exports.promiseReturningOperators = {
  every,
  forEach,
  reduce,
  toArray,
  some,
  find,
};
