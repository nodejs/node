'use strict';

const {
  DateNow,
  SafeMap,
  Symbol,
  globalThis,
} = primordials;

class Timers {
  constructor() {
    this.timers = new SafeMap();

    this.setTimeout = this.#createTimer.bind(this, false);
    this.clearTimeout = this.#clearTimer.bind(this);
    this.setInterval = this.#createTimer.bind(this, true);
    this.clearInterval = this.#clearTimer.bind(this);
  }

  #createTimer(isInterval, callback, delay, ...args) {
    const timerId = Symbol('kTimerId');
    const timer = {
      id: timerId,
      callback,
      runAt: DateNow() + delay,
      interval: isInterval,
      args,
    };
    this.timers.set(timerId, timer);
    return timerId;
  }

  #clearTimer(timerId) {
    this.timers.delete(timerId);
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

    for (const timer of timers.values()) {

      if (!(this.now >= timer.runAt)) continue;

      timer.callback(...timer.args);
      if (timer.interval) {
        timer.runAt = this.now + (timer.runAt - this.now) % timer.args[0];
        continue;
      }

      timers.delete(timer.id);
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
