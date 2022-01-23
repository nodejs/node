'use strict';

const { AbortController } = require('internal/abort_controller');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
  AbortError,
} = require('internal/errors');
const { validateInteger } = require('internal/validators');
const { kWeakHandler } = require('internal/event_target');

const {
  ArrayPrototypePush,
  MathFloor,
  Promise,
  PromiseReject,
  PromisePrototypeCatch,
  Symbol,
} = primordials;

const kEmpty = Symbol('kEmpty');
const kEof = Symbol('kEof');

async function * map(fn, options) {
  if (typeof fn !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'fn', ['Function', 'AsyncFunction'], fn);
  }

  if (options != null && typeof options !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('options', ['Object']);
  }

  let concurrency = 1;
  if (options?.concurrency != null) {
    concurrency = MathFloor(options.concurrency);
  }

  validateInteger(concurrency, 'concurrency', 1);

  const ac = new AbortController();
  const stream = this;
  const queue = [];
  const signal = ac.signal;
  const signalOpt = { signal };

  const abort = () => ac.abort();
  if (options?.signal?.aborted) {
    abort();
  }

  options?.signal?.addEventListener('abort', abort);

  let next;
  let resume;
  let done = false;

  function onDone() {
    done = true;
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
        } catch (err) {
          val = PromiseReject(err);
        }

        if (val === kEmpty) {
          continue;
        }

        if (typeof val?.catch === 'function') {
          val.catch(onDone);
        }

        queue.push(val);
        if (next) {
          next();
          next = null;
        }

        if (!done && queue.length && queue.length >= concurrency) {
          await new Promise((resolve) => {
            resume = resolve;
          });
        }
      }
      queue.push(kEof);
    } catch (err) {
      const val = PromiseReject(err);
      PromisePrototypeCatch(val, onDone);
      queue.push(val);
    } finally {
      done = true;
      if (next) {
        next();
        next = null;
      }
      options?.signal?.removeEventListener('abort', abort);
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
        if (resume) {
          resume();
          resume = null;
        }
      }

      await new Promise((resolve) => {
        next = resolve;
      });
    }
  } finally {
    ac.abort();

    done = true;
    if (resume) {
      resume();
      resume = null;
    }
  }
}

async function some(fn, options) {
  // https://tc39.es/proposal-iterator-helpers/#sec-iteratorprototype.some
  // Note that some does short circuit but also closes the iterator if it does
  const ac = new AbortController();
  if (options?.signal) {
    if (options.signal.aborted) {
      ac.abort();
    }
    options.signal.addEventListener('abort', () => ac.abort(), {
      [kWeakHandler]: this,
      once: true,
    });
  }
  const mapped = this.map(fn, { ...options, signal: ac.signal });
  for await (const result of mapped) {
    if (result) {
      ac.abort();
      return true;
    }
  }
  return false;
}

async function every(fn, options) {
  if (typeof fn !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'fn', ['Function', 'AsyncFunction'], fn);
  }
  // https://en.wikipedia.org/wiki/De_Morgan%27s_laws
  return !(await some.call(this, async (x) => {
    return !(await fn(x));
  }, options));
}

async function forEach(fn, options) {
  if (typeof fn !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'fn', ['Function', 'AsyncFunction'], fn);
  }
  async function forEachFn(value, options) {
    await fn(value, options);
    return kEmpty;
  }
  // eslint-disable-next-line no-unused-vars
  for await (const unused of this.map(forEachFn, options));
}

async function * filter(fn, options) {
  if (typeof fn !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'fn', ['Function', 'AsyncFunction'], fn);
  }
  async function filterFn(value, options) {
    if (await fn(value, options)) {
      return value;
    }
    return kEmpty;
  }
  yield* this.map(filterFn, options);
}

async function toArray(options) {
  const result = [];
  for await (const val of this) {
    if (options?.signal?.aborted) {
      throw new AbortError({ cause: options.signal.reason });
    }
    ArrayPrototypePush(result, val);
  }
  return result;
}

async function* flatMap(fn, options) {
  for await (const val of this.map(fn, options)) {
    yield* val;
  }
}

module.exports.streamReturningOperators = {
  filter,
  flatMap,
  map,
};

module.exports.promiseReturningOperators = {
  every,
  forEach,
  toArray,
  some,
};
