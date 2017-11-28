'use strict';

const async_wrap = process.binding('async_wrap');
// Two arrays that share state between C++ and JS.
const { async_hook_fields, async_id_fields } = async_wrap;
const {
  initTriggerId,
  // The needed emit*() functions.
  emitInit
} = require('internal/async_hooks');
// Grab the constants necessary for working with internal arrays.
const { kInit, kAsyncIdCounter } = async_wrap.constants;
// Symbols for storing async id state.
const async_id_symbol = Symbol('asyncId');
const trigger_async_id_symbol = Symbol('triggerId');

const errors = require('internal/errors');

// Timeout values > TIMEOUT_MAX are set to 1.
const TIMEOUT_MAX = 2147483647; // 2^31-1

module.exports = {
  TIMEOUT_MAX,
  kTimeout: Symbol('timeout'), // For hiding Timeouts on other internals.
  async_id_symbol,
  trigger_async_id_symbol,
  Timeout,
  setUnrefTimeout,
};

// Timer constructor function.
// The entire prototype is defined in lib/timers.js
function Timeout(after, callback, args) {
  this._called = false;
  this._idleTimeout = after;
  this._idlePrev = this;
  this._idleNext = this;
  this._idleStart = null;
  this._onTimeout = callback;
  this._timerArgs = args;
  this._repeat = null;
  this._destroyed = false;
  this[async_id_symbol] = ++async_id_fields[kAsyncIdCounter];
  this[trigger_async_id_symbol] = initTriggerId();
  if (async_hook_fields[kInit] > 0)
    emitInit(
      this[async_id_symbol], 'Timeout', this[trigger_async_id_symbol], this
    );
}

var timers;
function getTimers() {
  if (timers === undefined) {
    timers = require('timers');
  }
  return timers;
}

function setUnrefTimeout(callback, after) {
  // Type checking identical to setTimeout()
  if (typeof callback !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  after *= 1; // coalesce to number or NaN
  if (!(after >= 1 && after <= TIMEOUT_MAX)) {
    if (after > TIMEOUT_MAX) {
      process.emitWarning(`${after} does not fit into` +
                          ' a 32-bit signed integer.' +
                          '\nTimeout duration was set to 1.',
                          'TimeoutOverflowWarning');
    }
    after = 1; // schedule on next tick, follows browser behavior
  }

  const timer = new Timeout(after, callback, null);
  if (process.domain)
    timer.domain = process.domain;

  getTimers()._unrefActive(timer);

  return timer;
}
