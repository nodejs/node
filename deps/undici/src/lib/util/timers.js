'use strict';

// Fast timer internal clock
let fastNow = 0;
const RESOLUTION_MS = 1000;
const TICK_MS = (RESOLUTION_MS >> 1) - 1;

// Fast timer-related states
const NOT_IN_LIST = -2;
const TO_BE_CLEARED = -1;
const PENDING = 0;
const ACTIVE = 1;

const kFastTimer = Symbol('kFastTimer');
const fastTimers = [];
let fastNowTimeout;

// Handles the periodic update of fast timers
function onTick() {
  fastNow += TICK_MS;

  let idx = 0;
  let len = fastTimers.length;

  while (idx < len) {
    const timer = fastTimers[idx];

    if (timer._state === PENDING) {
      timer._idleStart = fastNow - TICK_MS;
      timer._state = ACTIVE;
    } else if (
      timer._state === ACTIVE &&
      fastNow >= timer._idleStart + timer._idleTimeout
    ) {
      timer._state = TO_BE_CLEARED;
      timer._idleStart = -1;
      timer._onTimeout(timer._timerArg);
    }

    if (timer._state === TO_BE_CLEARED) {
      timer._state = NOT_IN_LIST;
      if (--len !== 0) {
        fastTimers[idx] = fastTimers[len];
      }
    } else {
      ++idx;
    }
  }

  fastTimers.length = len;
  if (fastTimers.length !== 0) {
    refreshTimeout();
  }
}

// Refreshes the timer for periodic ticks
function refreshTimeout() {
  if (fastNowTimeout) {
    fastNowTimeout.refresh();
  } else {
    clearTimeout(fastNowTimeout);
    fastNowTimeout = setTimeout(onTick, TICK_MS);
    fastNowTimeout?.unref();
  }
}

class FastTimer {
  [kFastTimer] = true;
  _state = NOT_IN_LIST;
  _idleTimeout = -1;
  _idleStart = -1;
  _onTimeout;
  _timerArg;

  constructor(callback, delay, arg) {
    this._onTimeout = callback;
    this._idleTimeout = delay;
    this._timerArg = arg;
    this.refresh();
  }

  refresh() {
    if (this._state === NOT_IN_LIST) {
      fastTimers.push(this);
    }
    if (!fastNowTimeout || fastTimers.length === 1) {
      refreshTimeout();
    }
    this._state = PENDING;
  }

  clear() {
    this._state = TO_BE_CLEARED;
    this._idleStart = -1;
  }
}

module.exports = {
  setTimeout(callback, delay, arg) {
    return delay <= RESOLUTION_MS
      ? setTimeout(callback, delay, arg)
      : new FastTimer(callback, delay, arg);
  },

  clearTimeout(timeout) {
    if (timeout[kFastTimer]) {
      timeout.clear();
    } else {
      clearTimeout(timeout);
    }
  },

  setFastTimeout(callback, delay, arg) {
    return new FastTimer(callback, delay, arg);
  },

  clearFastTimeout(timeout) {
    timeout.clear();
  },

  now() {
    return fastNow;
  },

  tick(delay = 0) {
    fastNow += delay - RESOLUTION_MS + 1;
    onTick();
    onTick();
  },

  reset() {
    fastNow = 0;
    fastTimers.length = 0;
    clearTimeout(fastNowTimeout);
    fastNowTimeout = null;
  },

  kFastTimer,
};
