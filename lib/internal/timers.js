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
  ERR_INVALID_CALLBACK,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { validateNumber } = require('internal/validators');

const { inspect } = require('util');

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
    after = 1; // Schedule on next tick, follows browser behavior
  }

  this._idleTimeout = after;
  this._idlePrev = this;
  this._idleNext = this;
  this._idleStart = null;
  // This must be set to null first to avoid function tracking
  // on the hidden class, revisit in V8 versions after 6.2
  this._onTimeout = null;
  this._onTimeout = callback;
  this._timerArgs = args;
  this._repeat = isRepeat ? after : null;
  this._destroyed = false;

  this[kRefed] = null;

  initAsyncResource(this, 'Timeout');
}

// Make sure the linked list only shows the minimal necessary information.
Timeout.prototype[inspect.custom] = function(_, options) {
  return inspect(this, {
    ...options,
    // Only inspect one level.
    depth: 0,
    // It should not recurse.
    customInspect: false
  });
};

Timeout.prototype.refresh = function() {
  if (this[kRefed])
    getTimers().active(this);
  else
    getTimers()._unrefActive(this);

  return this;
};

function setUnrefTimeout(callback, after) {
  // Type checking identical to setTimeout()
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK();
  }

  const timer = new Timeout(callback, after, undefined, false);
  getTimers()._unrefActive(timer);

  return timer;
}

// Type checking used by timers.enroll() and Socket#setTimeout()
function validateTimerDuration(msecs) {
  validateNumber(msecs, 'msecs');
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
