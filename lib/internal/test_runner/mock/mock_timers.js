'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');

const {
  DateNow,
  FunctionPrototypeBind,
  globalThis,
  Promise,
} = primordials;

const PriorityQueue = require('internal/priority_queue');
const nodeTimers = require('timers');
const nodeTimersPromises = require('timers/promises');
const console = require('console')

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

  tick(time = 1) {
    // if (!this.isEnabled) {
    //   throw new Error('you should enable fakeTimers first by calling the .enable function');
    // }

   // Increase the current time by the specified time.
   this.#now += time;
    
    // Execute all timers whose runAt time is less than or equal to the current time.
    let timer = this.#timers.peek();
    while (timer && timer.runAt <= this.#now) {
      // Execute the timer's callback function with the specified arguments.
      timer.callback(...timer.args);

      // Remove the timer from the timer queue.
      this.#timers.shift();

      // If the timer is an interval timer, update its runAt time and re-insert it into the timer queue.
      if (timer.interval) {
        timer.runAt += timer.interval;
        this.#timers.insert(timer);
        return;
      }

      // Get the next timer in the timer queue.
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

    globalThis.setTimeout = this.#setTimeout;
    globalThis.clearTimeout = this.#clearTimeout;
    globalThis.setInterval = this.#setInterval;
    globalThis.clearInterval = this.#clearInterval;

    nodeTimers.setTimeout = this.#setTimeout;
    nodeTimers.clearTimeout = this.#clearTimeout;
    nodeTimers.setInterval = this.#setInterval;
    nodeTimers.clearInterval = this.#clearInterval;

    nodeTimersPromises.setTimeout = (ms) => new Promise(
      (resolve) => {
        const id = this.#setTimeout(() => resolve(id), ms);
      },
    );

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
