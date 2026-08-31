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


// Return native promises unchanged and normalize custom thenables exactly once.
// Capturing `then` avoids repeated getter access, while calling it with `value`
// as the receiver preserves thenables that depend on their `this` value.
function getThenablePromise(value) {
  if (isPromise(value)) {
    return value;
  }

  const valueType = typeof value;
  if ((valueType === 'object' && value !== null) || valueType === 'function') {
    const then = value.then;
    if (typeof then === 'function') {
      return PromiseResolve({
        __proto__: null,
        then(resolve, reject) {
          FunctionPrototypeCall(then, value, resolve, reject);
        },
      });
    }
  }

  return undefined;
}

function map(fn, options) {
  validateFunction(fn, 'fn');
  if (options != null) {
    validateObject(options, 'options');
  }
  if (options?.signal != null) {
    validateAbortSignal(options.signal, 'options.signal');
  }

  let concurrency = 1;
  if (options?.concurrency != null) {
    concurrency = MathFloor(options.concurrency);
  }

  let highWaterMark = concurrency - 1;
  if (options?.highWaterMark != null) {
    highWaterMark = MathFloor(options.highWaterMark);
  }

  validateInteger(concurrency, 'options.concurrency', 1);
  validateInteger(highWaterMark, 'options.highWaterMark', 0);

  highWaterMark += concurrency;

  return async function* map() {
    const signal = AbortSignal.any([options?.signal].filter(Boolean));
    const stream = this;
    const queue = [];
    const signalOpt = { signal };

    let next;
    let resume;
    let done = false;
    let cnt = 0;

    function onCatch() {
      done = true;
      afterItemProcessed();
    }

    function afterItemProcessed() {
      cnt -= 1;
      maybeResume();
    }

    function maybeResume() {
      if (
        resume &&
        !done &&
        cnt < concurrency &&
        queue.length < highWaterMark
      ) {
        resume();
        resume = null;
      }
    }

    async function pump() {
      try {
        for await (let val of stream) {
          if (done) {
            return;
          }

          if (signal.aborted) {
            throw new AbortError();
          }

          try {
            val = fn(val, signalOpt);

            if (val === kEmpty) {
              continue;
            }

            val = PromiseResolve(val);
          } catch (err) {
            val = PromiseReject(err);
          }

          cnt += 1;

          PromisePrototypeThen(val, afterItemProcessed, onCatch);

          queue.push(val);
          if (next) {
            next();
            next = null;
          }

          if (!done && (queue.length >= highWaterMark || cnt >= concurrency)) {
            await new Promise((resolve) => {
              resume = resolve;
            });
          }
        }
        queue.push(kEof);
      } catch (err) {
        const val = PromiseReject(err);
        PromisePrototypeThen(val, afterItemProcessed, onCatch);
        queue.push(val);
      } finally {
        done = true;
        if (next) {
          next();
          next = null;
        }
      }
    }

    pump();

    try {
      while (true) {
        while (queue.length > 0) {
          const val = await queue[0];

          if (val === kEof) {
            return;
          }

          if (signal.aborted) {
            throw new AbortError();
          }

          if (val !== kEmpty) {
            yield val;
          }

          queue.shift();
          maybeResume();
        }

        await new Promise((resolve) => {
          next = resolve;
        });
      }
    } finally {
      done = true;
      if (resume) {
        resume();
        resume = null;
      }
    }
  }.call(this);
}

function nowOrLater(fn, fn2, args) {
  const value = fn(...args);
  const promise = getThenablePromise(value);
  if (promise !== undefined) {
    return PromisePrototypeThen(promise, fn2);
  }
  return fn2(value);
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

  const ac = new AbortController();
  const predicateSignal = AbortSignal.any([ac.signal, signal].filter(Boolean));
  const predicateOptions = { signal: predicateSignal };

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
      matches = fn(chunk, predicateOptions);
      const matchesPromise = getThenablePromise(matches);
      if (matchesPromise !== undefined) {
        PromisePrototypeThen(
          matchesPromise,
          (result) => evaluationFinished(result, chunk, index),
          evaluationRejected,
        );
        return;
      }
    } catch (err) {
      evaluationRejected(err);
      return;
    }
    evaluationFinished(matches, chunk, index);
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
    ac.abort();

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
  async function filterFn(value, options) {
    if (await fn(value, options)) {
      return value;
    }
    return kEmpty;
  }
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
        try {
          result = reducer(initialValue, value, reducerOptions);
          const resultPromise = getThenablePromise(result);
          if (resultPromise === undefined) {
            initialValue = result;
            continue;
          }

          reducing = true;
          PromisePrototypeThen(resultPromise, reducerFulfilled, reducerRejected);
        } catch (err) {
          fail(err);
          break;
        }
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
  const stream = this;

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

  function onAbort() {
    stream.destroy(new AbortError(undefined, { cause: signal.reason }));
  }

  this.on('readable', onReadable);
  const done = finished(this, { writable: false, cleanup: true });
  if (signal != null) {
    signal.addEventListener('abort', onAbort, { once: true });
  }

  try {
    if (signal?.aborted) {
      onAbort();
    }
    await done;
  } finally {
    this.off('readable', onReadable);
    signal?.removeEventListener('abort', onAbort);
  }
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
  return async function* drop() {
    if (options?.signal?.aborted) {
      throw new AbortError();
    }
    for await (const val of this) {
      if (options?.signal?.aborted) {
        throw new AbortError();
      }
      if (number-- <= 0) {
        yield val;
      }
    }
  }.call(this);
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
