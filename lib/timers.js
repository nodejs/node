'use strict';

const Timer = process.binding('timer_wrap').Timer;
const kOnTimeout = Timer.kOnTimeout | 0;

// Timeout values > TIMEOUT_MAX are set to 1.
const TIMEOUT_MAX = 2147483647; // 2^31-1

// IDLE TIMEOUTS
//
// Because often many sockets will have the same idle timeout we will not
// use one timeout watcher per item. It is too much overhead.  Instead
// we'll use a single watcher for all sockets with the same timeout value
// and a set. This technique is described in the libev manual but using linked
// lists instead of sets:
// http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#Be_smart_about_timeouts

// Object containing all sets of timers
// key = time in milliseconds
// value = set
var lists = {};
var triggerTimers = new Map();

// the main function - creates lists on demand and the watchers associated
// with them.
function insert(item, msecs) {
  item._idleStart = Timer.now();
  item._idleTimeout = msecs;

  if (msecs < 0) return;

  var timer;

  if (! lists[msecs]) {
    timer = new Timer();
    timer.start(msecs, 0);
    timer.msecs = msecs;
    timer[kOnTimeout] = listOnTimeout;

    lists[msecs] = new Set();
    triggerTimers.set(msecs, timer);
  }

  lists[msecs].add(item);
}

function listOnTimeout() {
  var msecs = this.msecs;
  var list = this;

  var now = Timer.now();

  var diff, first, threw;
  if (lists[msecs]) {
    for (let first of lists[msecs]) {
      diff = now - first._idleStart;
      if (diff < msecs) {
        list.start(msecs - diff, 0);
        return;
      } else {
        lists[msecs].delete(first);
        if (!first._onTimeout) continue;

        // v0.4 compatibility: if the timer callback throws and the
        // domain or uncaughtException handler ignore the exception,
        // other timers that expire on this tick should still run.
        //
        // https://github.com/joyent/node/issues/2631
        var domain = first.domain;
        if (domain && domain._disposed)
          continue;

        try {
          if (domain)
            domain.enter();
          threw = true;
          first._called = true;
          first._onTimeout();
          if (domain)
            domain.exit();
          threw = false;
        } finally {
          if (threw) {
            // We need to continue processing after domain error handling
            // is complete, but not by using whatever domain was left over
            // when the timeout threw its exception.
            var oldDomain = process.domain;
            process.domain = null;
            process.nextTick(listOnTimeoutNT, list);
            process.domain = oldDomain;
          }
        }
      }
    }
  }

  list.close();
  if (lists[msecs] && lists[msecs].size === 0) {
    delete lists[msecs];
  }
}


function listOnTimeoutNT(list) {
  list[kOnTimeout]();
}


const unenroll = exports.unenroll = function(item) {

  var list = lists[item._idleTimeout];
  if (list) {
    list.delete(item);
  }

  // if empty then stop the watcher
  if (list && list.size === 0) {
    if (triggerTimers.has(item._idleTimeout)) {
      triggerTimers.get(item._idleTimeout).close();
      triggerTimers.delete(item._idleTimeout);
    }
    delete lists[item._idleTimeout];
  }
  // if active is called later, then we want to make sure not to insert again
  item._idleTimeout = -1;
};


// Does not start the time, just sets up the members needed.
exports.enroll = function(item, msecs) {
  if (typeof msecs !== 'number') {
    throw new TypeError('msecs must be a number');
  }

  if (msecs < 0 || !isFinite(msecs)) {
    throw new RangeError('msecs must be a non-negative finite number');
  }

  // if this item was already in a list somewhere
  // then we should unenroll it from that
  unenroll(item);

  // Ensure that msecs fits into signed int32
  if (msecs > TIMEOUT_MAX) {
    msecs = TIMEOUT_MAX;
  }

  item._idleTimeout = msecs;
  lists[msecs] = lists[msecs] || new Set();
  lists[msecs].add(item);
};


// call this whenever the item is active (not idle)
// it will reset its timeout.
exports.active = function(item) {
  var msecs = item._idleTimeout;
  if (msecs >= 0)
    insert(item, msecs);
};


/*
 * DOM-style timers
 */


exports.setTimeout = function(callback, after) {
  after *= 1; // coalesce to number or NaN

  if (!(after >= 1 && after <= TIMEOUT_MAX)) {
    after = 1; // schedule on next tick, follows browser behaviour
  }

  var timer = new Timeout(after);
  var length = arguments.length;
  var ontimeout = callback;
  switch (length) {
    // fast cases
    case 0:
    case 1:
    case 2:
      break;
    case 3:
      ontimeout = callback.bind(timer, arguments[2]);
      break;
    case 4:
      ontimeout = callback.bind(timer, arguments[2], arguments[3]);
      break;
    case 5:
      ontimeout =
          callback.bind(timer, arguments[2], arguments[3], arguments[4]);
      break;
    // slow case
    default:
      var args = new Array(length - 2);
      for (var i = 2; i < length; i++)
        args[i - 2] = arguments[i];
      ontimeout = callback.apply.bind(callback, timer, args);
      break;
  }
  timer._onTimeout = ontimeout;

  if (process.domain) timer.domain = process.domain;

  exports.active(timer);

  return timer;
};


exports.clearTimeout = function(timer) {
  if (timer && (timer[kOnTimeout] || timer._onTimeout)) {
    timer[kOnTimeout] = timer._onTimeout = null;
    if (timer instanceof Timeout) {
      timer.close(); // for after === 0
    } else {
      exports.unenroll(timer);
    }
  }
};


exports.setInterval = function(callback, repeat) {
  repeat *= 1; // coalesce to number or NaN

  if (!(repeat >= 1 && repeat <= TIMEOUT_MAX)) {
    repeat = 1; // schedule on next tick, follows browser behaviour
  }

  var timer = new Timeout(repeat);
  var length = arguments.length;
  var ontimeout = callback;
  switch (length) {
    case 0:
    case 1:
    case 2:
      break;
    case 3:
      ontimeout = callback.bind(timer, arguments[2]);
      break;
    case 4:
      ontimeout = callback.bind(timer, arguments[2], arguments[3]);
      break;
    case 5:
      ontimeout =
          callback.bind(timer, arguments[2], arguments[3], arguments[4]);
      break;
    default:
      var args = new Array(length - 2);
      for (var i = 2; i < length; i += 1)
        args[i - 2] = arguments[i];
      ontimeout = callback.apply.bind(callback, timer, args);
      break;
  }
  timer._onTimeout = wrapper;
  timer._repeat = ontimeout;

  if (process.domain) timer.domain = process.domain;
  exports.active(timer);

  return timer;

  function wrapper() {
    timer._repeat.call(this);

    // Timer might be closed - no point in restarting it
    if (!timer._repeat)
      return;

    // If timer is unref'd (or was - it's permanently removed from the list.)
    if (this._handle) {
      this._handle.start(repeat, 0);
    } else {
      timer._idleTimeout = repeat;
      exports.active(timer);
    }
  }
};


exports.clearInterval = function(timer) {
  if (timer && timer._repeat) {
    timer._repeat = null;
    clearTimeout(timer);
  }
};


const Timeout = function(after) {
  this._called = false;
  this._idleTimeout = after;
  this._idleStart = null;
  this._onTimeout = null;
  this._repeat = null;
};


function unrefdHandle() {
  this.owner._onTimeout();
  if (!this.owner._repeat)
    this.owner.close();
}


Timeout.prototype.unref = function() {
  if (this._handle) {
    this._handle.unref();
  } else if (typeof(this._onTimeout) === 'function') {
    var now = Timer.now();
    if (!this._idleStart) this._idleStart = now;
    var delay = this._idleStart + this._idleTimeout - now;
    if (delay < 0) delay = 0;
    exports.unenroll(this);

    // Prevent running cb again when unref() is called during the same cb
    if (this._called && !this._repeat) return;

    this._handle = new Timer();
    this._handle.owner = this;
    this._handle[kOnTimeout] = unrefdHandle;
    this._handle.start(delay, 0);
    this._handle.domain = this.domain;
    this._handle.unref();
  }
  return this;
};

Timeout.prototype.ref = function() {
  if (this._handle)
    this._handle.ref();
  return this;
};

Timeout.prototype.close = function() {
  this._onTimeout = null;
  if (this._handle) {
    this._handle[kOnTimeout] = null;
    this._handle.close();
  } else {
    exports.unenroll(this);
  }
  return this;
};


var immediateQueue = new Set();


function processImmediate() {
  var queue = immediateQueue;
  var domain, immediate;

  immediateQueue = new Set();

  for (let immediate of queue) {
    queue.delete(immediate);
    domain = immediate.domain;

    if (domain)
      domain.enter();

    var threw = true;
    try {
      if (immediate._onImmediate) {
        immediate._onImmediate();
      }
      threw = false;
    } finally {
      if (threw) {
        if (!queue.size === 0) {
          // Handle any remaining on next tick, assuming we're still
          // alive to do so.
          for (let item of immediateQueue) {
            queue.add(item);
          }
          immediateQueue = queue;
          process.nextTick(processImmediate);
        }
      }
    }

    if (domain)
      domain.exit();
  }

  // Only round-trip to C++ land if we have to. Calling clearImmediate() on an
  // immediate that's in |queue| is okay. Worst case is we make a superfluous
  // call to NeedImmediateCallbackSetter().
  if (immediateQueue.size === 0) {
    process._needImmediateCallback = false;
  }
}


function Immediate() { }

Immediate.prototype.domain = undefined;
Immediate.prototype._onImmediate = undefined;


exports.setImmediate = function(callback, arg1, arg2, arg3) {
  var i, args;
  var len = arguments.length;
  var immediate = new Immediate();

  switch (len) {
    // fast cases
    case 0:
    case 1:
      immediate._onImmediate = callback;
      break;
    case 2:
      immediate._onImmediate = function() {
        callback.call(immediate, arg1);
      };
      break;
    case 3:
      immediate._onImmediate = function() {
        callback.call(immediate, arg1, arg2);
      };
      break;
    case 4:
      immediate._onImmediate = function() {
        callback.call(immediate, arg1, arg2, arg3);
      };
      break;
    // slow case
    default:
      args = new Array(len - 1);
      for (i = 1; i < len; i++)
        args[i - 1] = arguments[i];

      immediate._onImmediate = function() {
        callback.apply(immediate, args);
      };
      break;
  }

  if (!process._needImmediateCallback) {
    process._needImmediateCallback = true;
    process._immediateCallback = processImmediate;
  }

  if (process.domain)
    immediate.domain = process.domain;

  immediateQueue.add(immediate);

  return immediate;
};


exports.clearImmediate = function(immediate) {
  if (!immediate) return;

  immediate._onImmediate = undefined;

  immediateQueue.delete(immediate);

  if (immediateQueue.size === 0) {
    process._needImmediateCallback = false;
  }
};


// Internal APIs that need timeouts should use timers._unrefActive instead of
// timers.active as internal timeouts shouldn't hold the loop open

var unrefList, unrefTimer;

function _makeTimerTimeout(timer) {
  var domain = timer.domain;
  var msecs = timer._idleTimeout;

  if (lists[msecs]) {
    lists[msecs].delete(timer);
  }
  unrefList.delete(timer);

  // Timer has been unenrolled by another timer that fired at the same time,
  // so don't make it timeout.
  if (msecs <= 0)
    return;

  if (!timer._onTimeout)
    return;

  if (domain) {
    if (domain._disposed)
      return;

    domain.enter();
  }

  timer._called = true;
  _runOnTimeout(timer);

  if (domain)
    domain.exit();
}

function _runOnTimeout(timer) {
  var threw = true;
  try {
    timer._onTimeout();
    threw = false;
  } finally {
    if (threw) process.nextTick(unrefTimeout);
  }
}

function unrefTimeout() {
  var now = Timer.now();

  var timeSinceLastActive;
  var nextTimeoutTime;
  var nextTimeoutDuration;
  var minNextTimeoutTime = TIMEOUT_MAX;
  var timersToTimeout = [];

  // The actual timer fired and has not yet been rearmed,
  // let's consider its next firing time is invalid for now.
  // It may be set to a relevant time in the future once
  // we scanned through the whole list of timeouts and if
  // we find a timeout that needs to expire.
  unrefTimer.when = -1;

  // Iterate over the list of timeouts,
  // call the onTimeout callback for those expired,
  // and rearm the actual timer if the next timeout to expire
  // will expire before the current actual timer.
  for (var cur of unrefList) {
    timeSinceLastActive = now - cur._idleStart;

    if (timeSinceLastActive < cur._idleTimeout) {
      // This timer hasn't expired yet, but check if its expiring time is
      // earlier than the actual timer's expiring time

      nextTimeoutDuration = cur._idleTimeout - timeSinceLastActive;
      nextTimeoutTime = now + nextTimeoutDuration;
      if (minNextTimeoutTime === TIMEOUT_MAX ||
          (nextTimeoutTime < minNextTimeoutTime)) {
        // We found a timeout that will expire earlier,
        // store its next timeout time now so that we
        // can rearm the actual timer accordingly when
        // we scanned through the whole list.
        minNextTimeoutTime = nextTimeoutTime;
      }
    } else {
      // We found a timer that expired. Do not call its _onTimeout callback
      // right now, as it could mutate any item of the list (including itself).
      // Instead, add it to another list that will be processed once the list
      // of current timers has been fully traversed.
      timersToTimeout.push(cur);
    }
  }

  var nbTimersToTimeout = timersToTimeout.length;
  for (var timerIdx = 0; timerIdx < nbTimersToTimeout; ++timerIdx)
    _makeTimerTimeout(timersToTimeout[timerIdx]);


  // Rearm the actual timer with the timeout delay
  // of the earliest timeout found.
  if (minNextTimeoutTime !== TIMEOUT_MAX) {
    unrefTimer.start(minNextTimeoutTime - now, 0);
    unrefTimer.when = minNextTimeoutTime;
  }
}


exports._unrefActive = function(item) {
  var msecs = item._idleTimeout;
  if (!msecs || msecs < 0) return;

  if (lists[msecs]) {
    lists[msecs].delete(item);
  }
  if (!unrefList) {
    unrefList = new Set();

    unrefTimer = new Timer();
    unrefTimer.unref();
    unrefTimer.when = -1;
    unrefTimer[kOnTimeout] = unrefTimeout;
  }

  var now = Timer.now();
  item._idleStart = now;

  var when = now + msecs;

  // If the actual timer is set to fire too late, or not set to fire at all,
  // we need to make it fire earlier
  if (unrefTimer.when === -1 || unrefTimer.when > when) {
    unrefTimer.start(msecs, 0);
    unrefTimer.when = when;
  }

  unrefList.add(item);
};
