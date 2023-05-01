'use strict';

const {
  DateNow,
  SafeSet,
  globalThis,
} = primordials;

const PriorityQueue = require('internal/priority_queue');

function compareTimersLists(a, b) {
  return (a.runAt - b.runAt) || (a.id - b.id);
}

function setPosition(node, pos) {
  node.priorityQueuePosition = pos;
}
class Timers {
  #currentTimer = 1;
  constructor() {
    this.timers = new PriorityQueue(compareTimersLists, setPosition);

    this.setTimeout = this.#createTimer.bind(this, false);
    this.clearTimeout = this.#clearTimer.bind(this);
    this.setInterval = this.#createTimer.bind(this, true);
    this.clearInterval = this.#clearTimer.bind(this);
  }

  #createTimer(isInterval, callback, delay, ...args) {
    const timerId = this.#currentTimer++;
    this.timers.insert({
      id: timerId,
      callback,
      runAt: DateNow() + delay,
      interval: isInterval,
      args,
    });

    return timerId;
  }

  #clearTimer(position) {
    this.timers.removeAt(position);
  }

}

let realSetTimeout;
let realClearTimeout;
let realSetInterval;
let realClearInterval;

class FakeTimers {
  constructor() {
    this.fakeTimers = {};
    this.isEnabled = false;
    this.now = DateNow();
  }

  tick(time = 0) {

    // if (!this.isEnabled) {
    //   throw new Error('you should enable fakeTimers first by calling the .enable function');
    // }

    this.now += time;
    const timers = this.fakeTimers.timers;
    const alreadyProcessed = new SafeSet();
    while (true) {
      const timer = timers.peek();

      if (!timer) {
        alreadyProcessed.clear();
        break;
      }

      if (alreadyProcessed.has(timer)) break;
      alreadyProcessed.add(timer);

      if (!(this.now >= timer.runAt)) continue;
      timer.callback(...timer.args);

      // if (timer.interval) {
      //   timer.runAt = this.now + (timer.runAt - this.now) % timer.args[0];
      //   continue;
      // }

      timers.removeAt(alreadyProcessed.size - 1);
    }
  }

  enable() {
    // if (this.isEnabled) {
    //   throw new Error('fakeTimers is already enabled!');
    // }
    this.now = DateNow();
    this.isEnabled = true;
    this.fakeTimers = new Timers();

    realSetTimeout = globalThis.setTimeout;
    realClearTimeout = globalThis.clearTimeout;
    realSetInterval = globalThis.setInterval;
    realClearInterval = globalThis.clearInterval;

    globalThis.setTimeout = this.fakeTimers.setTimeout;
    globalThis.clearTimeout = this.fakeTimers.clearTimeout;
    globalThis.setInterval = this.fakeTimers.setInterval;
    globalThis.clearInterval = this.fakeTimers.clearInterval;

    // this.#dispatchPendingTimers()
  }

  reset() {
    this.isEnabled = false;
    this.fakeTimers = {};

    // Restore the real timer functions
    globalThis.setTimeout = realSetTimeout;
    globalThis.clearTimeout = realClearTimeout;
    globalThis.setInterval = realSetInterval;
    globalThis.clearInterval = realClearInterval;
  }

  releaseAllTimers() {

  }
}

module.exports = { FakeTimers };
