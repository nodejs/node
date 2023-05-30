'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');

const {
  ArrayPrototypeAt,
  DateNow,
  FunctionPrototypeApply,
  FunctionPrototypeBind,
  Promise,
  PromiseReject,
  SymbolAsyncIterator,
  globalThis,
} = primordials;
const {
  validateAbortSignal,
} = require('internal/validators');
const {
  AbortError,
} = require('internal/errors');
const PriorityQueue = require('internal/priority_queue');
const nodeTimers = require('timers');
const nodeTimersPromises = require('timers/promises');
const EventEmitter = require('events');

function compareTimersLists(a, b) {
  return (a.runAt - b.runAt) || (a.id - b.id);
}

function setPosition(node, pos) {
  node.priorityQueuePosition = pos;
}
const abortIt = (signal) => new AbortError(undefined, { cause: signal.reason });

class MockTimers {
  #realSetTimeout;
  #realClearTimeout;
  #realSetInterval;
  #realClearInterval;

  #realPromisifiedSetTimeout;
  #realPromisifiedSetInterval;

  #realTimersSetTimeout;
  #realTimersClearTimeout;
  #realTimersSetInterval;
  #realTimersClearInterval;

  #isEnabled = false;
  #currentTimer = 1;
  #now = DateNow();

  #timers = new PriorityQueue(compareTimersLists, setPosition);

  #setTimeout = FunctionPrototypeBind(this.#createTimer, this, false);
  #clearTimeout = FunctionPrototypeBind(this.#clearTimer, this);
  #setInterval = FunctionPrototypeBind(this.#createTimer, this, true);
  #clearInterval = FunctionPrototypeBind(this.#clearTimer, this);

  constructor() {
    emitExperimentalWarning('The MockTimers API');
  }

  #createTimer(isInterval, callback, delay, ...args) {
    const timerId = this.#currentTimer++;
    this.#timers.insert({
      __proto__: null,
      id: timerId,
      callback,
      runAt: DateNow() + delay,
      interval: isInterval,
      args,
    });

    return timerId;
  }

  #clearTimer(position) {
    this.#timers.removeAt(position);
  }

  async * #setIntervalPromisified(interval, startTime, options) {
    const context = this;
    const emitter = new EventEmitter();
    if (options?.signal) {
      try {
        validateAbortSignal(options.signal, 'options.signal');
      } catch (err) {
        return PromiseReject(err);
      }

      if (options.signal?.aborted) {
        return PromiseReject(abortIt(options.signal));
      }

      const onabort = (reason) => {
        emitter.emit('data', { aborted: true, reason });
        options?.signal?.removeEventListener('abort', onabort);
      };

      options.signal?.addEventListener('abort', onabort);
    }

    const eventIt = EventEmitter.on(emitter, 'data');
    const callback = () => {
      startTime += interval;
      emitter.emit('data', startTime);
    };

    const timerId = this.#createTimer(true, callback, interval, options);

    const iterator = {
      __proto__: null,
      [SymbolAsyncIterator]() {
        return this;
      },
      async next() {
        const result = await eventIt.next();
        const value = ArrayPrototypeAt(result.value, 0);
        if (value?.aborted) {
          iterator.return();
          return PromiseReject(abortIt(options.signal));
        }

        return {
          __proto__: null,
          done: result.done,
          value,
        };
      },
      async return() {
        emitter.removeAllListeners();
        context.#clearTimer(timerId);
        return eventIt.return();
      },
    };

    yield* iterator;
  }

  #setTimeoutPromisified(ms, result, options) {
    return new Promise((resolve, reject) => {
      if (options?.signal) {
        try {
          validateAbortSignal(options.signal, 'options.signal');
        } catch (err) {
          return reject(err);
        }

        if (options.signal?.aborted) {
          return reject(abortIt(options.signal));
        }
      }

      const onabort = () => {
        this.#clearTimeout(id);
        return reject(abortIt(options.signal));
      };

      const id = this.#setTimeout(() => {
        options?.signal?.removeEventListener('abort', onabort);
        return resolve(result || id);
      }, ms);

      options?.signal?.addEventListener('abort', onabort);
    });
  }

  tick(time = 1) {
    // if (!this.isEnabled) {
    //   throw new Error('you should enable fakeTimers first by calling the .enable function');
    // }

    this.#now += time;

    let timer = this.#timers.peek();
    while (timer && timer.runAt <= this.#now) {
      FunctionPrototypeApply(timer.callback, undefined, timer.args);

      this.#timers.shift();

      if (timer.interval) {
        timer.runAt += timer.interval;
        this.#timers.insert(timer);
        return;
      }

      timer = this.#timers.peek();
    }

  }

  enable() {
    // if (this.isEnabled) {
    //   throw new Error('fakeTimers is already enabled!');
    // }
    this.#now = DateNow();
    this.#isEnabled = true;

    this.#realSetTimeout = globalThis.setTimeout;
    this.#realClearTimeout = globalThis.clearTimeout;
    this.#realSetInterval = globalThis.setInterval;
    this.#realClearInterval = globalThis.clearInterval;

    this.#realTimersSetTimeout = nodeTimers.setTimeout;
    this.#realTimersClearTimeout = nodeTimers.clearTimeout;
    this.#realTimersSetInterval = nodeTimers.setInterval;
    this.#realTimersClearInterval = nodeTimers.clearInterval;

    this.#realPromisifiedSetTimeout = nodeTimersPromises.setTimeout;
    this.#realPromisifiedSetInterval = nodeTimersPromises.setInterval;

    globalThis.setTimeout = this.#setTimeout;
    globalThis.clearTimeout = this.#clearTimeout;
    globalThis.setInterval = this.#setInterval;
    globalThis.clearInterval = this.#clearInterval;

    nodeTimers.setTimeout = this.#setTimeout;
    nodeTimers.clearTimeout = this.#clearTimeout;
    nodeTimers.setInterval = this.#setInterval;
    nodeTimers.clearInterval = this.#clearInterval;

    nodeTimersPromises.setTimeout = FunctionPrototypeBind(this.#setTimeoutPromisified, this);
    nodeTimersPromises.setInterval = FunctionPrototypeBind(this.#setIntervalPromisified, this);
  }

  reset() {
    this.#isEnabled = false;

    // Restore the real timer functions
    globalThis.setTimeout = this.#realSetTimeout;
    globalThis.clearTimeout = this.#realClearTimeout;
    globalThis.setInterval = this.#realSetInterval;
    globalThis.clearInterval = this.#realClearInterval;

    nodeTimers.setTimeout = this.#realTimersSetTimeout;
    nodeTimers.clearTimeout = this.#realTimersClearTimeout;
    nodeTimers.setInterval = this.#realTimersSetInterval;
    nodeTimers.clearInterval = this.#realTimersClearInterval;

    nodeTimersPromises.setTimeout = this.#realPromisifiedSetTimeout;
    nodeTimersPromises.setInterval = this.#realPromisifiedSetInterval;

    let timer = this.#timers.peek();
    while (timer) {
      this.#timers.shift();
      timer = this.#timers.peek();
    }
  }

  releaseAllTimers() {

  }
}

module.exports = { MockTimers };
