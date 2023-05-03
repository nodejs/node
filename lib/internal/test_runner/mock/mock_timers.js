'use strict';

const {
  DateNow,
  FunctionPrototypeBind,
  globalThis,
  Promise,
} = primordials;

const PriorityQueue = require('internal/priority_queue');
const nodeTimers = require('timers');
const nodeTimersPromises = require('timers/promises');

function compareTimersLists(a, b) {
  return (a.runAt - b.runAt) || (a.id - b.id);
}

function setPosition(node, pos) {
  node.priorityQueuePosition = pos;
}


class MockTimers {
  #realSetTimeout;
  #realClearTimeout;
  #realSetInterval;
  #realClearInterval;

  #realPromisifiedSetTimeout;

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

  #createTimer(isInterval, callback, delay, ...args) {
    const timerId = this.#currentTimer++;
    this.#timers.insert({
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

  tick(time = 1) {

    // if (!this.isEnabled) {
    //   throw new Error('you should enable fakeTimers first by calling the .enable function');
    // }
    this.#now += time;
    const timers = this.#timers;

    for (let timer = timers.peek(); timer?.runAt <= this.#now; timer = timers.peek()) {
      timer.callback(...timer.args);
      timers.shift();
      if (timer.interval) {
        timer.runAt = timer.runAt + timer.interval;
        timers.insert(timer);
      }
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

    globalThis.setTimeout = this.#setTimeout;
    globalThis.clearTimeout = this.#clearTimeout;
    globalThis.setInterval = this.#setInterval;
    globalThis.clearInterval = this.#clearInterval;

    nodeTimers.setTimeout = this.#setTimeout;
    nodeTimers.clearTimeout = this.#clearTimeout;
    nodeTimers.setInterval = this.#setInterval;
    nodeTimers.clearInterval = this.#clearInterval;

    nodeTimersPromises.setTimeout = (ms) => new Promise(
      (resolve) => this.#setTimeout(resolve, ms),
    );

  }

  reset() {
    this.#isEnabled = false;

    // Restore the real timer functions
    globalThis.setTimeout = this.#realSetTimeout;
    globalThis.clearTimeout = this.#realClearTimeout;
    globalThis.setInterval = this.#realSetInterval;
    globalThis.clearInterval = this.#realClearInterval;
    nodeTimersPromises.setTimeout = this.#realPromisifiedSetTimeout;

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
