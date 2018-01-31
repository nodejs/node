'use strict';

const async_wrap = process.binding('async_wrap');
// Two arrays that share state between C++ and JS.
const { async_hook_fields, async_id_fields } = async_wrap;
const {
  getDefaultTriggerAsyncId,
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
const TIMEOUT_MAX = 2 ** 31 - 1;

const refreshFnSymbol = Symbol('refresh()');
const unrefedSymbol = Symbol('unrefed');

module.exports = {
  TIMEOUT_MAX,
  kTimeout: Symbol('timeout'), // For hiding Timeouts on other internals.
  async_id_symbol,
  trigger_async_id_symbol,
  Timeout,
  refreshFnSymbol,
  setUnrefTimeout,
  validateTimerDuration
};

var timers;
function getTimers() {
  if (timers === undefined) {
    timers = require('timers');
  }
  return timers;
}

// Timer constructor function.
// The entire prototype is defined in lib/timers.js
function Timeout(callback, after, args, isRepeat, isUnrefed) {
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

  this._called = false;
  this._idleTimeout = after;
  this._idlePrev = this;
  this._idleNext = this;
  this._idleStart = null;
  // this must be set to null first to avoid function tracking
  // on the hidden class, revisit in V8 versions after 6.2
  this._onTimeout = null;
  this._onTimeout = callback;
  this._timerArgs = args;
  this._repeat = isRepeat ? after : null;
  this._destroyed = false;

  this[unrefedSymbol] = isUnrefed;

  this[async_id_symbol] = ++async_id_fields[kAsyncIdCounter];
  this[trigger_async_id_symbol] = getDefaultTriggerAsyncId();
  if (async_hook_fields[kInit] > 0) {
    emitInit(this[async_id_symbol],
             'Timeout',
             this[trigger_async_id_symbol],
             this);
  }
}

Timeout.prototype[refreshFnSymbol] = function refresh() {
  if (this._handle) {
    // Would be more ideal with uv_timer_again(), however that API does not
    // cause libuv's sorted timers data structure (a binary heap at the time
    // of writing) to re-sort itself. This causes ordering inconsistencies.
    this._handle.stop();
    this._handle.start(this._idleTimeout);
  } else if (this[unrefedSymbol]) {
    getTimers()._unrefActive(this);
  } else {
    getTimers().active(this);
  }
};

function setUnrefTimeout(callback, after, arg1, arg2, arg3) {
  // Type checking identical to setTimeout()
  if (typeof callback !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  let i, args;
  switch (arguments.length) {
    // fast cases
    case 1:
    case 2:
      break;
    case 3:
      args = [arg1];
      break;
    case 4:
      args = [arg1, arg2];
      break;
    default:
      args = [arg1, arg2, arg3];
      for (i = 5; i < arguments.length; i++) {
        // extend array dynamically, makes .apply run much faster in v6.0.0
        args[i - 2] = arguments[i];
      }
      break;
  }

  const timer = new Timeout(callback, after, args, false, true);
  getTimers()._unrefActive(timer);

  return timer;
}

// Type checking used by timers.enroll() and Socket#setTimeout()
function validateTimerDuration(msecs) {
  if (typeof msecs !== 'number') {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'msecs',
                               'number', msecs);
  }

  if (msecs < 0 || !isFinite(msecs)) {
    throw new errors.RangeError('ERR_OUT_OF_RANGE', 'msecs',
                                'a non-negative finite number', msecs);
  }

  // Ensure that msecs fits into signed int32
  if (msecs > TIMEOUT_MAX) {
    process.emitWarning(`${msecs} does not fit into a 32-bit signed integer.` +
                        `\nTimer duration was truncated to ${TIMEOUT_MAX}.`,
                        'TimeoutOverflowWarning');
    return TIMEOUT_MAX;
  }

  return msecs;
}
