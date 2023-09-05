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
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
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
          this.#storeOriginalSetTimeout();

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
          this.#storeOriginalSetInterval();

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
          this.#storeOriginalSetImmediate();

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
          this.#restoreOriginalSetTimeout();
        },
        setInterval: () => {
          this.#restoreOriginalSetInterval();
        },
        setImmediate: () => {
          this.#restoreSetImmediate();
        },
      },
    };

    const target = activate ? options.toFake : options.toReal;
    ArrayPrototypeForEach(this.#timersInContext, (timer) => target[timer]());
    this.#isEnabled = activate;
  }

  #restoreSetImmediate() {
    ObjectDefineProperty(
      globalThis,
      'setImmediate',
      this.#realSetImmediate,
    );
    ObjectDefineProperty(
      globalThis,
      'clearImmediate',
      this.#realClearImmediate,
    );
    ObjectDefineProperty(
      nodeTimers,
      'setImmediate',
      this.#realTimersSetImmediate,
    );
    ObjectDefineProperty(
      nodeTimers,
      'clearImmediate',
      this.#realTimersClearImmediate,
    );
    ObjectDefineProperty(
      nodeTimersPromises,
      'setImmediate',
      this.#realPromisifiedSetImmediate,
    );
  }

  #restoreOriginalSetInterval() {
    ObjectDefineProperty(
      globalThis,
      'setInterval',
      this.#realSetInterval,
    );
    ObjectDefineProperty(
      globalThis,
      'clearInterval',
      this.#realClearInterval,
    );
    ObjectDefineProperty(
      nodeTimers,
      'setInterval',
      this.#realTimersSetInterval,
    );
    ObjectDefineProperty(
      nodeTimers,
      'clearInterval',
      this.#realTimersClearInterval,
    );
    ObjectDefineProperty(
      nodeTimersPromises,
      'setInterval',
      this.#realPromisifiedSetInterval,
    );
  }

  #restoreOriginalSetTimeout() {
    ObjectDefineProperty(
      globalThis,
      'setTimeout',
      this.#realSetTimeout,
    );
    ObjectDefineProperty(
      globalThis,
      'clearTimeout',
      this.#realClearTimeout,
    );
    ObjectDefineProperty(
      nodeTimers,
      'setTimeout',
      this.#realTimersSetTimeout,
    );
    ObjectDefineProperty(
      nodeTimers,
      'clearTimeout',
      this.#realTimersClearTimeout,
    );
    ObjectDefineProperty(
      nodeTimersPromises,
      'setTimeout',
      this.#realPromisifiedSetTimeout,
    );
  }

  #storeOriginalSetImmediate() {
    this.#realSetImmediate = ObjectGetOwnPropertyDescriptor(
      globalThis,
      'setImmediate',
    );
    this.#realClearImmediate = ObjectGetOwnPropertyDescriptor(
      globalThis,
      'clearImmediate',
    );
    this.#realTimersSetImmediate = ObjectGetOwnPropertyDescriptor(
      nodeTimers,
      'setImmediate',
    );
    this.#realTimersClearImmediate = ObjectGetOwnPropertyDescriptor(
      nodeTimers,
      'clearImmediate',
    );
    this.#realPromisifiedSetImmediate = ObjectGetOwnPropertyDescriptor(
      nodeTimersPromises,
      'setImmediate',
    );
  }

  #storeOriginalSetInterval() {
    this.#realSetInterval = ObjectGetOwnPropertyDescriptor(
      globalThis,
      'setInterval',
    );
    this.#realClearInterval = ObjectGetOwnPropertyDescriptor(
      globalThis,
      'clearInterval',
    );
    this.#realTimersSetInterval = ObjectGetOwnPropertyDescriptor(
      nodeTimers,
      'setInterval',
    );
    this.#realTimersClearInterval = ObjectGetOwnPropertyDescriptor(
      nodeTimers,
      'clearInterval',
    );
    this.#realPromisifiedSetInterval = ObjectGetOwnPropertyDescriptor(
      nodeTimersPromises,
      'setInterval',
    );
  }

  #storeOriginalSetTimeout() {
    this.#realSetTimeout = ObjectGetOwnPropertyDescriptor(
      globalThis,
      'setTimeout',
    );
    this.#realClearTimeout = ObjectGetOwnPropertyDescriptor(
      globalThis,
      'clearTimeout',
    );
    this.#realTimersSetTimeout = ObjectGetOwnPropertyDescriptor(
      nodeTimers,
      'setTimeout',
    );
    this.#realTimersClearTimeout = ObjectGetOwnPropertyDescriptor(
      nodeTimers,
      'clearTimeout',
    );
    this.#realPromisifiedSetTimeout = ObjectGetOwnPropertyDescriptor(
      nodeTimersPromises,
      'setTimeout',
    );
  }

  /**
   * Advances the virtual time of MockTimers by the specified duration (in milliseconds).
   * This method simulates the passage of time and triggers any scheduled timers that are due.
   * @param {number} [time=1] - The amount of time (in milliseconds) to advance the virtual time.
   * @throws {ERR_INVALID_STATE} If MockTimers are not enabled.
   * @throws {ERR_INVALID_ARG_VALUE} If a negative time value is provided.
   */
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

  /**
   * Enables MockTimers for the specified timers.
   * @param {string[]} timers - An array of timer types to enable, e.g., ['setTimeout', 'setInterval'].
   * @throws {ERR_INVALID_STATE} If MockTimers are already enabled.
   * @throws {ERR_INVALID_ARG_VALUE} If an unsupported timer type is specified.
   */
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

  /**
   * An alias for `this.reset()`, allowing the disposal of the `MockTimers` instance.
   */
  [SymbolDispose]() {
    this.reset();
  }

  /**
   * Resets MockTimers, disabling any enabled timers and clearing the execution queue.
   * Does nothing if MockTimers are not enabled.
   */
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

  /**
   * Runs all scheduled timers until there are no more pending timers.
   * @throws {ERR_INVALID_STATE} If MockTimers are not enabled.
   */
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
