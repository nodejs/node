'use strict';

const {
  getDefaultTriggerAsyncId,
  newAsyncId,
  initHooksExist,
  emitInit
} = require('internal/async_hooks');
// Symbols for storing async id state.
const async_id_symbol = Symbol('asyncId');
const trigger_async_id_symbol = Symbol('triggerId');

const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;

// Timeout values > TIMEOUT_MAX are set to 1.
const TIMEOUT_MAX = 2 ** 31 - 1;

const kRefed = Symbol('refed');

module.exports = {
  TIMEOUT_MAX,
  kTimeout: Symbol('timeout'), // For hiding Timeouts on other internals.
  async_id_symbol,
  trigger_async_id_symbol,
  Timeout,
  kRefed,
  initAsyncResource,
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

function initAsyncResource(resource, type) {
  const asyncId = resource[async_id_symbol] = newAsyncId();
  const triggerAsyncId =
    resource[trigger_async_id_symbol] = getDefaultTriggerAsyncId();
  if (initHooksExist())
    emitInit(asyncId, type, triggerAsyncId, resource);
}

// Timer constructor function.
// The entire prototype is defined in lib/timers.js
function Timeout(callback, after, args, isRepeat) {
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

  this[kRefed] = null;

  initAsyncResource(this, 'Timeout');
}

Timeout.prototype.refresh = function() {
  if (this[kRefed])
    getTimers().active(this);
  else
    getTimers()._unrefActive(this);

  return this;
};

function setUnrefTimeout(callback, after, arg1, arg2, arg3) {
  // Type checking identical to setTimeout()
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK();
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

  const timer = new Timeout(callback, after, args, false);
  getTimers()._unrefActive(timer);

  return timer;
}

// Type checking used by timers.enroll() and Socket#setTimeout()
function validateTimerDuration(msecs) {
  if (typeof msecs !== 'number') {
    throw new ERR_INVALID_ARG_TYPE('msecs', 'number', msecs);
  }

  if (msecs < 0 || !isFinite(msecs)) {
    throw new ERR_OUT_OF_RANGE('msecs', 'a non-negative finite number', msecs);
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
