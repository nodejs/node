'use strict';

const TimerWrap = process.binding('timer_wrap').Timer;
const L = require('internal/linkedlist');
const assert = require('assert');
const util = require('util');
const debug = util.debuglog('timer');
const kOnTimeout = TimerWrap.kOnTimeout | 0;

// Timeout values > TIMEOUT_MAX are set to 1.
const TIMEOUT_MAX = 2147483647; // 2^31-1

// IDLE TIMEOUTS
// Object maps containing linked lists of timers, keyed and sorted by their
// duration in milliseconds.
//
// Because often many sockets will have the same idle timeout we will not
// use one timeout watcher per item. It is too much overhead.  Instead
// we'll use a single watcher for all sockets with the same timeout value
// and a linked list. This technique is described in the libev manual:
// http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#Be_smart_about_timeouts

const refedLists = {};
const unrefedLists = {};


// Schedule or re-schedule a timer.
// The item must have been enroll()'d first.
exports.active = function(item) {
  insert(item, false);
};

// Internal APIs that need timeouts should use `_unrefActive()` instead of
// `active()` so that they do not unnecessarily keep the process open.
exports._unrefActive = function(item) {
  insert(item, true);
};


// The underlying logic for scheduling or re-scheduling a timer.
function insert(item, unrefed) {
  const msecs = item._idleTimeout;
  if (msecs < 0 || msecs === undefined) return;

  item._idleStart = TimerWrap.now();

  const lists = unrefed === true ? unrefedLists : refedLists;

  var list = lists[msecs];
  if (!list) {
    debug('no %d list was found in insert, creating a new one', msecs);
    list = new TimersList(msecs, unrefed);
    L.init(list);
    list._timer._list = list;

    if (unrefed === true) list._timer.unref();
    list._timer.start(msecs, 0);

    lists[msecs] = list;
    list._timer[kOnTimeout] = listOnTimeout;
  }

  L.append(list, item);
  assert(!L.isEmpty(list)); // list is not empty
}

function TimersList(msecs, unrefed) {
  this._idleNext = null; // Create the list with the linkedlist properties to
  this._idlePrev = null; // prevent any unnecessary hidden class changes.
  this._timer = new TimerWrap();
  this._unrefed = unrefed;
  this.msecs = msecs;
}

function listOnTimeout() {
  var list = this._list;
  var msecs = list.msecs;

  debug('timeout callback %d', msecs);

  var now = TimerWrap.now();
  debug('now: %s', now);

  var diff, timer;
  while (timer = L.peek(list)) {
    diff = now - timer._idleStart;

    if (diff < msecs) {
      this.start(msecs - diff, 0);
      debug('%d list wait because diff is %d', msecs, diff);
      return;
    }


    L.remove(timer);
    assert(timer !== L.peek(list));

    if (!timer._onTimeout) continue;

    var domain = timer.domain;
    if (domain) {

      // If the timer callback throws and the
      // domain or uncaughtException handler ignore the exception,
      // other timers that expire on this tick should still run.
      //
      // https://github.com/nodejs/node-v0.x-archive/issues/2631
      if (domain._disposed)
        continue;

      domain.enter();
    }

    tryOnTimeout(timer, list);

    if (domain)
      domain.exit();
  }

  debug('%d list empty', msecs);
  assert(L.isEmpty(list));
  this.close();
  if (list._unrefed === true) {
    delete unrefedLists[msecs];
  } else {
    delete refedLists[msecs];
  }
}


// An optimization so that the try/finally only de-optimizes what is in this
// smaller function.
function tryOnTimeout(timer, list) {
  timer._called = true;
  var threw = true;
  try {
    timer._onTimeout();
    threw = false;
  } finally {
    if (!threw) return;

    // We need to continue processing after domain error handling
    // is complete, but not by using whatever domain was left over
    // when the timeout threw its exception.
    const domain = process.domain;
    process.domain = null;
    process.nextTick(listOnTimeoutNT, list);
    process.domain = domain;
  }
}


function listOnTimeoutNT(list) {
  list._timer[kOnTimeout]();
}


function reuse(item) {
  L.remove(item);

  var list = refedLists[item._idleTimeout];
  // if empty - reuse the watcher
  if (list && L.isEmpty(list)) {
    debug('reuse hit');
    list._timer.stop();
    delete refedLists[item._idleTimeout];
    return list._timer;
  }

  return null;
}


const unenroll = exports.unenroll = function(item) {
  var handle = reuse(item);
  if (handle) {
    debug('unenroll: list empty');
    handle.close();
  }
  // if active is called later, then we want to make sure not to insert again
  item._idleTimeout = -1;
};


// Does not start the time, just sets up the members needed.
exports.enroll = function(item, msecs) {
  if (typeof msecs !== 'number') {
    throw new TypeError('"msecs" argument must be a number');
  }

  if (msecs < 0 || !isFinite(msecs)) {
    throw new RangeError('"msecs" argument must be ' +
                         'a non-negative finite number');
  }

  // if this item was already in a list somewhere
  // then we should unenroll it from that
  if (item._idleNext) unenroll(item);

  // Ensure that msecs fits into signed int32
  if (msecs > TIMEOUT_MAX) {
    msecs = TIMEOUT_MAX;
  }

  item._idleTimeout = msecs;
  L.init(item);
};


/*
 * DOM-style timers
 */


exports.setTimeout = function(callback, after) {
  if (typeof callback !== 'function') {
    throw new TypeError('"callback" argument must be a function');
  }

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
      ontimeout = () => callback.call(timer, arguments[2]);
      break;
    case 4:
      ontimeout = () => callback.call(timer, arguments[2], arguments[3]);
      break;
    case 5:
      ontimeout =
        () => callback.call(timer, arguments[2], arguments[3], arguments[4]);
      break;
    // slow case
    default:
      var args = new Array(length - 2);
      for (var i = 2; i < length; i++)
        args[i - 2] = arguments[i];
      ontimeout = () => callback.apply(timer, args);
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
  if (typeof callback !== 'function') {
    throw new TypeError('"callback" argument must be a function');
  }

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
      ontimeout = () => callback.call(timer, arguments[2]);
      break;
    case 4:
      ontimeout = () => callback.call(timer, arguments[2], arguments[3]);
      break;
    case 5:
      ontimeout =
        () => callback.call(timer, arguments[2], arguments[3], arguments[4]);
      break;
    default:
      var args = new Array(length - 2);
      for (var i = 2; i < length; i += 1)
        args[i - 2] = arguments[i];
      ontimeout = () => callback.apply(timer, args);
      break;
  }
  timer._onTimeout = wrapper;
  timer._repeat = ontimeout;

  if (process.domain) timer.domain = process.domain;
  exports.active(timer);

  return timer;

  function wrapper() {
    timer._repeat();

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
  this._idlePrev = this;
  this._idleNext = this;
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
  } else if (typeof this._onTimeout === 'function') {
    var now = TimerWrap.now();
    if (!this._idleStart) this._idleStart = now;
    var delay = this._idleStart + this._idleTimeout - now;
    if (delay < 0) delay = 0;

    // Prevent running cb again when unref() is called during the same cb
    if (this._called && !this._repeat) {
      exports.unenroll(this);
      return;
    }

    var handle = reuse(this);

    this._handle = handle || new TimerWrap();
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


var immediateQueue = {};
L.init(immediateQueue);


function processImmediate() {
  var queue = immediateQueue;
  var domain, immediate;

  immediateQueue = {};
  L.init(immediateQueue);

  while (L.isEmpty(queue) === false) {
    immediate = L.shift(queue);
    domain = immediate.domain;

    if (domain)
      domain.enter();

    var threw = true;
    try {
      immediate._onImmediate();
      threw = false;
    } finally {
      if (threw) {
        if (!L.isEmpty(queue)) {
          // Handle any remaining on next tick, assuming we're still
          // alive to do so.
          while (!L.isEmpty(immediateQueue)) {
            L.append(queue, L.shift(immediateQueue));
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
  if (L.isEmpty(immediateQueue)) {
    process._needImmediateCallback = false;
  }
}


function Immediate() { }

Immediate.prototype.domain = undefined;
Immediate.prototype._onImmediate = undefined;
Immediate.prototype._idleNext = undefined;
Immediate.prototype._idlePrev = undefined;


exports.setImmediate = function(callback, arg1, arg2, arg3) {
  if (typeof callback !== 'function') {
    throw new TypeError('"callback" argument must be a function');
  }

  var i, args;
  var len = arguments.length;
  var immediate = new Immediate();

  L.init(immediate);

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

  L.append(immediateQueue, immediate);

  return immediate;
};


exports.clearImmediate = function(immediate) {
  if (!immediate) return;

  immediate._onImmediate = undefined;

  L.remove(immediate);

  if (L.isEmpty(immediateQueue)) {
    process._needImmediateCallback = false;
  }
};
