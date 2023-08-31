'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');

const {
  ArrayPrototypeForEach,
  ArrayPrototypeIncludes,
  DateNow,
  FunctionPrototypeApply,
  FunctionPrototypeBind,
  Promise,
  SymbolAsyncIterator,
  SymbolDispose,
  globalThis,
} = primordials;
const {
  validateAbortSignal,
  validateArray,
} = require('internal/validators');

const {
  AbortError,
  codes: {
    ERR_INVALID_STATE,
    ERR_INVALID_ARG_VALUE,
  },
} = require('internal/errors');

const PriorityQueue = require('internal/priority_queue');
const nodeTimers = require('timers');
const nodeTimersPromises = require('timers/promises');
const EventEmitter = require('events');

let kResistStopPropagation;

function compareTimersLists(a, b) {
  return (a.runAt - b.runAt) || (a.id - b.id);
}

function setPosition(node, pos) {
  node.priorityQueuePosition = pos;
}

function abortIt(signal) {
  return new AbortError(undefined, { __proto__: null, cause: signal.reason });
}

const SUPPORTED_TIMERS = ['setTimeout', 'setInterval', 'setImmediate'];
const TIMERS_DEFAULT_INTERVAL = {
  __proto__: null,
  setImmediate: -1,
};

class MockTimers {
  #realSetTimeout;
  #realClearTimeout;
  #realSetInterval;
  #realClearInterval;
  #realSetImmediate;
  #realClearImmediate;

  #realPromisifiedSetTimeout;
  #realPromisifiedSetInterval;

  #realTimersSetTimeout;
  #realTimersClearTimeout;
  #realTimersSetInterval;
  #realTimersClearInterval;
  #realTimersSetImmediate;
  #realTimersClearImmediate;
  #realPromisifiedSetImmediate;

  #timersInContext = [];
  #isEnabled = false;
  #currentTimer = 1;
  #now = DateNow();

  #executionQueue = new PriorityQueue(compareTimersLists, setPosition);

  #setTimeout = FunctionPrototypeBind(this.#createTimer, this, false);
  #clearTimeout = FunctionPrototypeBind(this.#clearTimer, this);
  #setInterval = FunctionPrototypeBind(this.#createTimer, this, true);
  #clearInterval = FunctionPrototypeBind(this.#clearTimer, this);

  #setImmediate = (callback, ...args) => {
    return this.#createTimer(
      false,
      callback,
      TIMERS_DEFAULT_INTERVAL.setImmediate,
      ...args,
    );
  };

  #clearImmediate = FunctionPrototypeBind(this.#clearTimer, this);
  constructor() {
    emitExperimentalWarning('The MockTimers API');
  }

  #createTimer(isInterval, callback, delay, ...args) {
    const timerId = this.#currentTimer++;
    this.#executionQueue.insert({
      __proto__: null,
      id: timerId,
      callback,
      runAt: this.#now + delay,
      interval: isInterval,
      args,
    });

    return timerId;
  }

  #clearTimer(position) {
    this.#executionQueue.removeAt(position);
  }

  async * #setIntervalPromisified(interval, startTime, options) {
    const context = this;
    const emitter = new EventEmitter();
    if (options?.signal) {
      validateAbortSignal(options.signal, 'options.signal');

      if (options.signal.aborted) {
        throw abortIt(options.signal);
      }

      const onAbort = (reason) => {
        emitter.emit('data', { __proto__: null, aborted: true, reason });
      };

      kResistStopPropagation ??= require('internal/event_target').kResistStopPropagation;
      options.signal.addEventListener('abort', onAbort, {
        __proto__: null,
        once: true,
        [kResistStopPropagation]: true,
      });
    }

    const eventIt = EventEmitter.on(emitter, 'data');
    const callback = () => {
      startTime += interval;
      emitter.emit('data', startTime);
    };

    const timerId = this.#createTimer(true, callback, interval, options);
    const clearListeners = () => {
      emitter.removeAllListeners();
      context.#clearTimer(timerId);
    };
    const iterator = {
      __proto__: null,
      [SymbolAsyncIterator]() {
        return this;
      },
      async next() {
        const result = await eventIt.next();
        const value = result.value[0];
        if (value?.aborted) {
          iterator.return();
          throw abortIt(options.signal);
        }

        return {
          __proto__: null,
          done: result.done,
          value,
        };
      },
      async return() {
        clearListeners();
        return eventIt.return();
      },
    };
    yield* iterator;
  }

  #promisifyTimer({ timerFn, clearFn, ms, result, options }) {
    return new Promise((resolve, reject) => {
      if (options?.signal) {
        try {
          validateAbortSignal(options.signal, 'options.signal');
        } catch (err) {
          return reject(err);
        }

        if (options.signal.aborted) {
          return reject(abortIt(options.signal));
        }
      }

      const onabort = () => {
        clearFn(id);
        return reject(abortIt(options.signal));
      };

      const id = timerFn(() => {
        return resolve(result);
      }, ms);

      if (options?.signal) {
        kResistStopPropagation ??= require('internal/event_target').kResistStopPropagation;
        options.signal.addEventListener('abort', onabort, {
          __proto__: null,
          once: true,
          [kResistStopPropagation]: true,
        });
      }
    });
  }

  #setImmediatePromisified(result, options) {
    return this.#promisifyTimer({
      __proto__: null,
      timerFn: FunctionPrototypeBind(this.#setImmediate, this),
      clearFn: FunctionPrototypeBind(this.#clearImmediate, this),
      ms: TIMERS_DEFAULT_INTERVAL.setImmediate,
      result,
      options,
    });
  }

  #setTimeoutPromisified(ms, result, options) {
    return this.#promisifyTimer({
      __proto__: null,
      timerFn: FunctionPrototypeBind(this.#setTimeout, this),
      clearFn: FunctionPrototypeBind(this.#clearTimeout, this),
      ms,
      result,
      options,
    });
  }

  #toggleEnableTimers(activate) {
    const options = {
      __proto__: null,
      toFake: {
        __proto__: null,
        setTimeout: () => {
          this.#realSetTimeout = globalThis.setTimeout;
          this.#realClearTimeout = globalThis.clearTimeout;
          this.#realTimersSetTimeout = nodeTimers.setTimeout;
          this.#realTimersClearTimeout = nodeTimers.clearTimeout;
          this.#realPromisifiedSetTimeout = nodeTimersPromises.setTimeout;

          globalThis.setTimeout = this.#setTimeout;
          globalThis.clearTimeout = this.#clearTimeout;

          nodeTimers.setTimeout = this.#setTimeout;
          nodeTimers.clearTimeout = this.#clearTimeout;

          nodeTimersPromises.setTimeout = FunctionPrototypeBind(
            this.#setTimeoutPromisified,
            this,
          );
        },
        setInterval: () => {
          this.#realSetInterval = globalThis.setInterval;
          this.#realClearInterval = globalThis.clearInterval;
          this.#realTimersSetInterval = nodeTimers.setInterval;
          this.#realTimersClearInterval = nodeTimers.clearInterval;
          this.#realPromisifiedSetInterval = nodeTimersPromises.setInterval;

          globalThis.setInterval = this.#setInterval;
          globalThis.clearInterval = this.#clearInterval;

          nodeTimers.setInterval = this.#setInterval;
          nodeTimers.clearInterval = this.#clearInterval;

          nodeTimersPromises.setInterval = FunctionPrototypeBind(
            this.#setIntervalPromisified,
            this,
          );
        },
        setImmediate: () => {
          this.#realSetImmediate = globalThis.setImmediate;
          this.#realClearImmediate = globalThis.clearImmediate;
          this.#realTimersSetImmediate = nodeTimers.setImmediate;
          this.#realTimersClearImmediate = nodeTimers.clearImmediate;

          globalThis.setImmediate = this.#setImmediate;
          globalThis.clearImmediate = this.#clearImmediate;

          nodeTimers.setImmediate = this.#setImmediate;
          nodeTimers.clearImmediate = this.#clearImmediate;

          nodeTimersPromises.setImmediate = FunctionPrototypeBind(
            this.#setImmediatePromisified,
            this,
          );
        },
      },
      toReal: {
        __proto__: null,
        setTimeout: () => {
          globalThis.setTimeout = this.#realSetTimeout;
          globalThis.clearTimeout = this.#realClearTimeout;

          nodeTimers.setTimeout = this.#realTimersSetTimeout;
          nodeTimers.clearTimeout = this.#realTimersClearTimeout;

          nodeTimersPromises.setTimeout = this.#realPromisifiedSetTimeout;
        },
        setInterval: () => {
          globalThis.setInterval = this.#realSetInterval;
          globalThis.clearInterval = this.#realClearInterval;

          nodeTimers.setInterval = this.#realTimersSetInterval;
          nodeTimers.clearInterval = this.#realTimersClearInterval;

          nodeTimersPromises.setInterval = this.#realPromisifiedSetInterval;
        },
        setImmediate: () => {
          globalThis.setImmediate = this.#realSetImmediate;
          globalThis.clearImmediate = this.#realClearImmediate;

          nodeTimers.setImmediate = this.#realTimersSetImmediate;
          nodeTimers.clearImmediate = this.#realTimersClearImmediate;

          nodeTimersPromises.setImmediate = this.#realPromisifiedSetImmediate;
        },
      },
    };

    const target = activate ? options.toFake : options.toReal;
    ArrayPrototypeForEach(this.#timersInContext, (timer) => target[timer]());
    this.#isEnabled = activate;
  }

  tick(time = 1) {
    if (!this.#isEnabled) {
      throw new ERR_INVALID_STATE(
        'You should enable MockTimers first by calling the .enable function',
      );
    }

    if (time < 0) {
      throw new ERR_INVALID_ARG_VALUE(
        'time',
        'positive integer',
        time,
      );
    }

    this.#now += time;
    let timer = this.#executionQueue.peek();
    while (timer) {
      if (timer.runAt > this.#now) break;
      FunctionPrototypeApply(timer.callback, undefined, timer.args);

      this.#executionQueue.shift();

      if (timer.interval) {
        timer.runAt += timer.interval;
        this.#executionQueue.insert(timer);
        return;
      }

      timer = this.#executionQueue.peek();
    }
  }

  enable(timers = SUPPORTED_TIMERS) {
    if (this.#isEnabled) {
      throw new ERR_INVALID_STATE(
        'MockTimers is already enabled!',
      );
    }

    validateArray(timers, 'timers');

    // Check that the timers passed are supported
    ArrayPrototypeForEach(timers, (timer) => {
      if (!ArrayPrototypeIncludes(SUPPORTED_TIMERS, timer)) {
        throw new ERR_INVALID_ARG_VALUE(
          'timers',
          timer,
          `option ${timer} is not supported`,
        );
      }
    });

    this.#timersInContext = timers;
    this.#now = DateNow();
    this.#toggleEnableTimers(true);
  }

  [SymbolDispose]() {
    this.reset();
  }

  reset() {
    // Ignore if not enabled
    if (!this.#isEnabled) return;

    this.#toggleEnableTimers(false);
    this.#timersInContext = [];

    let timer = this.#executionQueue.peek();
    while (timer) {
      this.#executionQueue.shift();
      timer = this.#executionQueue.peek();
    }
  }

  runAll() {
    if (!this.#isEnabled) {
      throw new ERR_INVALID_STATE(
        'You should enable MockTimers first by calling the .enable function',
      );
    }

    this.tick(Infinity);
  }
}

module.exports = { MockTimers };
