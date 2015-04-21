// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var Timer = process.binding('timer_wrap').Timer;
var L = require('_linklist');
var assert = require('assert').ok;

// Timeout values > TIMEOUT_MAX are set to 1.
var TIMEOUT_MAX = 2147483647; // 2^31-1

var debug;
if (process.env.NODE_DEBUG && /timer/.test(process.env.NODE_DEBUG)) {
  debug = function() { require('util').error.apply(this, arguments); };
} else {
  debug = function() { };
}


// IDLE TIMEOUTS
//
// Because often many sockets will have the same idle timeout we will not
// use one timeout watcher per item. It is too much overhead.  Instead
// we'll use a single watcher for all sockets with the same timeout value
// and a linked list. This technique is described in the libev manual:
// http://pod.tst.eu/http://cvs.schmorp.de/libev/ev.pod#Be_smart_about_timeouts

// Object containing all lists, timers
// key = time in milliseconds
// value = list
var lists = {};

// the main function - creates lists on demand and the watchers associated
// with them.
function insert(item, msecs) {
  item._idleStart = Date.now();
  item._monotonicStartTime = Timer.now();

  item._idleTimeout = msecs;

  if (msecs < 0) return;

  var list;

  if (lists[msecs]) {
    list = lists[msecs];
  } else {
    list = new Timer();
    list.start(msecs, 0);

    L.init(list);

    lists[msecs] = list;
    list.msecs = msecs;
    list.ontimeout = listOnTimeout;
  }

  L.append(list, item);
  assert(!L.isEmpty(list)); // list is not empty
}

function listOnTimeout() {
  var msecs = this.msecs;
  var list = this;

  debug('timeout callback ' + msecs);

  var now = Timer.now();
  debug('now: %d', now);

  var first;
  while (first = L.peek(list)) {
    // If the previous iteration caused a timer to be added,
    // update the value of "now" so that timing computations are
    // done correctly. See test/simple/test-timers-blocking-callback.js
    // for more information.
    if (now < first._monotonicStartTime) {
      now = Timer.now();
      debug('now: %d', now);
    }

    var diff = now - first._monotonicStartTime;
    if (diff < msecs) {
      list.start(msecs - diff, 0);
      debug(msecs + ' list wait because diff is ' + diff);
      return;
    } else {
      L.remove(first);
      assert(first !== L.peek(list));

      if (!first._onTimeout) continue;

      // v0.4 compatibility: if the timer callback throws and the
      // domain or uncaughtException handler ignore the exception,
      // other timers that expire on this tick should still run.
      //
      // https://github.com/joyent/node/issues/2631
      var domain = first.domain;
      if (domain && domain._disposed) continue;
      try {
        if (domain)
          domain.enter();
        var threw = true;
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
          process.nextTick(function() {
            list.ontimeout();
          });
          process.domain = oldDomain;
        }
      }
    }
  }

  debug(msecs + ' list empty');
  assert(L.isEmpty(list));
  list.close();
  delete lists[msecs];
}


var unenroll = exports.unenroll = function(item) {
  L.remove(item);

  var list = lists[item._idleTimeout];
  // if empty then stop the watcher
  debug('unenroll');
  if (list && L.isEmpty(list)) {
    debug('unenroll: list empty');
    list.close();
    delete lists[item._idleTimeout];
  }
  // if active is called later, then we want to make sure not to insert again
  item._idleTimeout = -1;
};


// Does not start the time, just sets up the members needed.
exports.enroll = function(item, msecs) {
  // if this item was already in a list somewhere
  // then we should unenroll it from that
  if (item._idleNext) unenroll(item);

  // Ensure that msecs fits into signed int32
  if (msecs > 0x7fffffff) {
    msecs = 0x7fffffff;
  }

  item._idleTimeout = msecs;
  L.init(item);
};


// call this whenever the item is active (not idle)
// it will reset its timeout.
exports.active = function(item) {
  var msecs = item._idleTimeout;
  if (msecs >= 0) {

    var list = lists[msecs];
    if (!list || L.isEmpty(list)) {
      insert(item, msecs);
    } else {
      item._idleStart = Date.now();
      item._monotonicStartTime = Timer.now();
      L.append(list, item);
    }
  }
};


/*
 * DOM-style timers
 */


exports.setTimeout = function(callback, after) {
  var timer;

  after *= 1; // coalesce to number or NaN

  if (!(after >= 1 && after <= TIMEOUT_MAX)) {
    after = 1; // schedule on next tick, follows browser behaviour
  }

  timer = new Timeout(after);

  if (arguments.length <= 2) {
    timer._onTimeout = callback;
  } else {
    /*
     * Sometimes setTimeout is called with arguments, EG
     *
     *   setTimeout(callback, 2000, "hello", "world")
     *
     * If that's the case we need to call the callback with
     * those args. The overhead of an extra closure is not
     * desired in the normal case.
     */
    var args = Array.prototype.slice.call(arguments, 2);
    timer._onTimeout = function() {
      callback.apply(timer, args);
    }
  }

  if (process.domain) timer.domain = process.domain;

  exports.active(timer);

  return timer;
};


exports.clearTimeout = function(timer) {
  if (timer && (timer.ontimeout || timer._onTimeout)) {
    timer.ontimeout = timer._onTimeout = null;
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
  var args = Array.prototype.slice.call(arguments, 2);
  timer._onTimeout = wrapper;
  timer._repeat = true;

  if (process.domain) timer.domain = process.domain;
  exports.active(timer);

  return timer;

  function wrapper() {
    callback.apply(this, args);
    // If callback called clearInterval().
    if (timer._repeat === false) return;
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
    timer._repeat = false;
    clearTimeout(timer);
  }
};


var Timeout = function(after) {
  this._idleTimeout = after;
  this._idlePrev = this;
  this._idleNext = this;
  this._idleStart = null;
  this._monotonicStartTime = null;
  this._onTimeout = null;
  this._repeat = false;
};


function unrefdHandle() {
  this.owner._onTimeout();
  if (!this.owner._repeat)
    this.owner.close();
}


Timeout.prototype.unref = function() {
  if (!this._handle) {

    var nowMonotonic = Timer.now();
    if (!this._idleStart || !this._monotonicStartTime) {
      this._idleStart = Date.now();
      this._monotonicStartTime = nowMonotonic;
    }

    var delay = this._monotonicStartTime + this._idleTimeout - nowMonotonic;
    if (delay < 0) delay = 0;
    exports.unenroll(this);
    this._handle = new Timer();
    this._handle.owner = this;
    this._handle.ontimeout = unrefdHandle;
    this._handle.start(delay, 0);
    this._handle.domain = this.domain;
    this._handle.unref();
  } else {
    this._handle.unref();
  }
};

Timeout.prototype.ref = function() {
  if (this._handle)
    this._handle.ref();
};

Timeout.prototype.close = function() {
  this._onTimeout = null;
  if (this._handle) {
    this._handle.ontimeout = null;
    this._handle.close();
  } else {
    exports.unenroll(this);
  }
};


var immediateQueue = {};
L.init(immediateQueue);


function processImmediate() {
  var immediate = L.shift(immediateQueue);

  if (L.isEmpty(immediateQueue)) {
    process._needImmediateCallback = false;
  }

  if (immediate._onImmediate) {
    if (immediate.domain) immediate.domain.enter();

    immediate._onImmediate();

    if (immediate.domain) immediate.domain.exit();
  }
}


exports.setImmediate = function(callback) {
  var immediate = {}, args;

  L.init(immediate);

  immediate._onImmediate = callback;

  if (arguments.length > 1) {
    args = Array.prototype.slice.call(arguments, 1);

    immediate._onImmediate = function() {
      callback.apply(immediate, args);
    };
  }

  if (!process._needImmediateCallback) {
    process._needImmediateCallback = true;
    process._immediateCallback = processImmediate;
  }

  if (process.domain) immediate.domain = process.domain;

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


// Internal APIs that need timeouts should use timers._unrefActive isntead of
// timers.active as internal timeouts shouldn't hold the loop open

var unrefList, unrefTimer;

function _makeTimerTimeout(timer) {
  var domain = timer.domain;
  var msecs = timer._idleTimeout;

  // Timer has been unenrolled by another timer that fired at the same time,
  // so don't make it timeout.
  if (!msecs || msecs < 0)
    return;

  if (!timer._onTimeout)
    return;

  if (domain && domain._disposed)
    return;

  try {
    var threw = true;

    if (domain) domain.enter();

    debug('unreftimer firing timeout');
    L.remove(timer);
    timer._onTimeout();

    threw = false;

    if (domain)
      domain.exit();
  } finally {
    if (threw) process.nextTick(unrefTimeout);
  }
}

function unrefTimeout() {
  var now = Timer.now();

  debug('unrefTimer fired');

  var timeSinceLastActive;
  var nextTimeoutTime;
  var nextTimeoutDuration;
  var minNextTimeoutTime;
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
  var cur = unrefList._idlePrev;
  while (cur != unrefList) {
    timeSinceLastActive = now - cur._monotonicStartTime;

    if (timeSinceLastActive < cur._idleTimeout) {
      // This timer hasn't expired yet, but check if its expiring time is
      // earlier than the actual timer's expiring time

      nextTimeoutDuration = cur._idleTimeout - timeSinceLastActive;
      nextTimeoutTime = now + nextTimeoutDuration;
      if (minNextTimeoutTime == null ||
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

    cur = cur._idlePrev;
  }

  var nbTimersToTimeout = timersToTimeout.length;
  for (var timerIdx = 0; timerIdx < nbTimersToTimeout; ++timerIdx)
    _makeTimerTimeout(timersToTimeout[timerIdx]);


  // Rearm the actual timer with the timeout delay
  // of the earliest timeout found.
  if (minNextTimeoutTime != null) {
    unrefTimer.start(minNextTimeoutTime - now, 0);
    unrefTimer.when = minNextTimeoutTime;
    debug('unrefTimer rescheduled');
  } else if (L.isEmpty(unrefList)) {
    debug('unrefList is empty');
  }
}


exports._unrefActive = function(item) {
  var msecs = item._idleTimeout;
  if (!msecs || msecs < 0) return;
  assert(msecs >= 0);

  L.remove(item);

  if (!unrefList) {
    debug('unrefList initialized');
    unrefList = {};
    L.init(unrefList);

    debug('unrefTimer initialized');
    unrefTimer = new Timer();
    unrefTimer.unref();
    unrefTimer.when = -1;
    unrefTimer.ontimeout = unrefTimeout;
  }

  var nowDate = Date.now();
  var nowMonotonicTimestamp = Timer.now();

  item._idleStart = nowDate;
  item._monotonicStartTime = nowMonotonicTimestamp;

  var when = nowMonotonicTimestamp + msecs;

  // If the actual timer is set to fire too late, or not set to fire at all,
  // we need to make it fire earlier
  if (unrefTimer.when === -1 || unrefTimer.when > when) {
    unrefTimer.start(msecs, 0);
    unrefTimer.when = when;
    debug('unrefTimer scheduled');
  }

  debug('unrefList append to end');
  L.append(unrefList, item);
};
