'use strict';

const { AbortController } = require('internal/abort_controller');
const { kWeakHandler } = require('internal/event_target');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
  }, AbortError,
} = require('internal/errors');

const {
  MathFloor,
  NumberIsInteger,
  Promise,
  PromiseReject,
  Symbol,
} = primordials;

const kEmpty = Symbol('kEmpty');
const kEof = Symbol('kEof');

async function * map(fn, options) {
  if (typeof fn !== 'function') {
    throw new ERR_INVALID_ARG_TYPE(
      'fn', ['Function', 'AsyncFunction'], this);
  }

  if (options != null && typeof options !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('options', ['Object']);
  }

  let concurrency = 1;
  if (options?.concurrency != null) {
    concurrency = MathFloor(options.concurrency);
  }

  if (!NumberIsInteger(concurrency)) {
    throw new ERR_INVALID_ARG_TYPE(
      'concurrency', ['Integer'], concurrency);
  }

  if (concurrency < 1) {
    throw new ERR_INVALID_ARG_VALUE('concurrency', concurrency);
  }

  const ac = new AbortController();
  const stream = this;
  const queue = [];
  const signal = ac.signal;
  const signalOpt = { signal };

  options?.signal?.addEventListener('abort', () => ac.abort(), {
    [kWeakHandler]: signalOpt
  });

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

        if (!done && queue.length >= concurrency) {
          await new Promise((resolve) => {
            resume = resolve;
          });
        }
      }
      queue.push(kEof);
    } catch (err) {
      const val = PromiseReject(err);
      val.catch(onDone);
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
        let val = queue[0];

        if (val === kEof) {
          return;
        }

        val = await val;

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

module.exports = {
  map
};
