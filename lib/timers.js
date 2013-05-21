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

var debug = require('util').debuglog('timer');


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

  debug('timeout callback %d', msecs);

  var now = Date.now();
  debug('now: %s', now);

  var first;
  while (first = L.peek(list)) {
    var diff = now - first._idleStart;
    if (diff < msecs) {
      list.start(msecs - diff, 0);
      debug('%d list wait because diff is %d', msecs, diff);
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
          process.nextTick(function() {
            list.ontimeout();
          });
        }
      }
    }
  }

  debug('%d list empty', msecs);
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
  this._onTimeout = null;
  this._repeat = false;
};

Timeout.prototype.unref = function() {
  if (!this._handle) {
    var now = Date.now();
    if (!this._idleStart) this._idleStart = now;
    var delay = this._idleStart + this._idleTimeout - now;
    if (delay < 0) delay = 0;
    exports.unenroll(this);
    this._handle = new Timer();
    this._handle.ontimeout = this._onTimeout;
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
