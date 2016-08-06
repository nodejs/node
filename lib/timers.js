'use strict';

const TimerWrap = process.binding('timer_wrap').Timer;
const L = require('internal/linkedlist');
const assert = require('assert');
const util = require('util');
const debug = util.debuglog('timer');
const kOnTimeout = TimerWrap.kOnTimeout | 0;

// Timeout values > TIMEOUT_MAX are set to 1.
const TIMEOUT_MAX = 2147483647; // 2^31-1


// HOW and WHY the timers implementation works the way it does.
//
// Timers are crucial to Node.js. Internally, any TCP I/O connection creates a
// timer so that we can time out of connections. Additionally, many user
// user libraries and applications also use timers. As such there may be a
// significantly large amount of timeouts scheduled at any given time.
// Therefore, it is very important that the timers implementation is performant
// and efficient.
//
// Note: It is suggested you first read though the lib/internal/linkedlist.js
// linked list implementation, since timers depend on it extensively. It can be
// somewhat counter-intuitive at first, as it is not actually a class. Instead,
// it is a set of helpers that operate on an existing object.
//
// In order to be as performant as possible, the architecture and data
// structures are designed so that they are optimized to handle the following
// use cases as efficiently as possible:

// - Adding a new timer. (insert)
// - Removing an existing timer. (remove)
// - Handling a timer timing out. (timeout)
//
// Whenever possible, the implementation tries to make the complexity of these
// operations as close to constant-time as possible.
// (So that performance is not impacted by the number of scheduled timers.)
//
// Object maps are kept which contain linked lists keyed by their duration in
// milliseconds.
// The linked lists within also have some meta-properties, one of which is a
// TimerWrap C++ handle, which makes the call after the duration to process the
// list it is attached to.
//
//
// ╔════ > Object Map
// ║
// ╠══
// ║ refedLists: { '40': { }, '320': { etc } } (keys of millisecond duration)
// ╚══          ┌─────────┘
//              │
// ╔══          │
// ║ TimersList { _idleNext: { }, _idlePrev: (self), _timer: (TimerWrap) }
// ║         ┌────────────────┘
// ║    ╔══  │                              ^
// ║    ║    { _idleNext: { },  _idlePrev: { }, _onTimeout: (callback) }
// ║    ║      ┌───────────┘
// ║    ║      │                                  ^
// ║    ║      { _idleNext: { etc },  _idlePrev: { }, _onTimeout: (callback) }
// ╠══  ╠══
// ║    ║
// ║    ╚════ >  Actual JavaScript timeouts
// ║
// ╚════ > Linked List
//
//
// With this, virtually constant-time insertion (append), removal, and timeout
// is possible in the JavaScript layer. Any one list of timers is able to be
// sorted by just appending to it because all timers within share the same
// duration. Therefore, any timer added later will always have been scheduled to
// timeout later, thus only needing to be appended.
// Removal from an object-property linked list is also virtually constant-time
// as can be seen in the lib/internal/linkedlist.js implementation.
// Timeouts only need to process any timers due to currently timeout, which will
// always be at the beginning of the list for reasons stated above. Any timers
// after the first one encountered that does not yet need to timeout will also
// always be due to timeout at a later time.
//
// Less-than constant time operations are thus contained in two places:
// TimerWrap's backing libuv timers implementation (a performant heap-based
// queue), and the object map lookup of a specific list by the duration of
// timers within (or creation of a new list).
// However, these operations combined have shown to be trivial in comparison to
// other alternative timers architectures.


// Object maps containing linked lists of timers, keyed and sorted by their
// duration in milliseconds.
//
// The difference between these two objects is that the former contains timers
// that will keep the process open if they are the only thing left, while the
// latter will not.
//
// - key = time in milliseconds
// - value = linked list
const refedLists = {};
const unrefedLists = {};


// Schedule or re-schedule a timer.
// The item must have been enroll()'d first.
const active = exports.active = function(item) {
  insert(item, false);
};

// Internal APIs that need timeouts should use `_unrefActive()` instead of
// `active()` so that they do not unnecessarily keep the process open.
exports._unrefActive = function(item) {
  insert(item, true);
};


// The underlying logic for scheduling or re-scheduling a timer.
//
// Appends a timer onto the end of an existing timers list, or creates a new
// TimerWrap backed list if one does not already exist for the specified timeout
// duration.
function insert(item, unrefed) {
  const msecs = item._idleTimeout;
  if (msecs < 0 || msecs === undefined) return;

  item._idleStart = TimerWrap.now();

  const lists = unrefed === true ? unrefedLists : refedLists;

  // Use an existing list if there is one, otherwise we need to make a new one.
  var list = lists[msecs];
  if (!list) {
    debug('no %d list was found in insert, creating a new one', msecs);
    // Make a new linked list of timers, and create a TimerWrap to schedule
    // processing for the list.
    list = new TimersList(msecs, unrefed);
    L.init(list);
    list._timer._list = list;

    if (unrefed === true) list._timer.unref();
    list._timer.start(msecs);

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
  debug('now: %d', now);

  var diff, timer;
  while (timer = L.peek(list)) {
    diff = now - timer._idleStart;

    // Check if this loop iteration is too early for the next timer.
    // This happens if there are more timers scheduled for later in the list.
    if (diff < msecs) {
      var timeRemaining = msecs - (TimerWrap.now() - timer._idleStart);
      if (timeRemaining < 0) {
        timeRemaining = 0;
      }
      this.start(timeRemaining);
      debug('%d list wait because diff is %d', msecs, diff);
      return;
    }

    // The actual logic for when a timeout happens.

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

  // If `L.peek(list)` returned nothing, the list was either empty or we have
  // called all of the timer timeouts.
  // As such, we can remove the list and clean up the TimerWrap C++ handle.
  debug('%d list empty', msecs);
  assert(L.isEmpty(list));
  this.close();

  // Either refedLists[msecs] or unrefedLists[msecs] may have been removed and
  // recreated since the reference to `list` was created. Make sure they're
  // the same instance of the list before destroying.
  if (list._unrefed === true && list === unrefedLists[msecs]) {
    delete unrefedLists[msecs];
  } else if (list === refedLists[msecs]) {
    delete refedLists[msecs];
  }
}


// An optimization so that the try/finally only de-optimizes (since at least v8
// 4.7) what is in this smaller function.
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
    // If we threw, we need to process the rest of the list in nextTick.
    process.nextTick(listOnTimeoutNT, list);
    process.domain = domain;
  }
}


function listOnTimeoutNT(list) {
  list._timer[kOnTimeout]();
}


// A convenience function for re-using TimerWrap handles more easily.
//
// This mostly exists to fix https://github.com/nodejs/node/issues/1264.
// Handles in libuv take at least one `uv_run` to be registered as unreferenced.
// Re-using an existing handle allows us to skip that, so that a second `uv_run`
// will return no active handles, even when running `setTimeout(fn).unref()`.
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


// Remove a timer. Cancels the timeout and resets the relevant timer properties.
const unenroll = exports.unenroll = function(item) {
  var handle = reuse(item);
  if (handle) {
    debug('unenroll: list empty');
    handle.close();
  }
  // if active is called later, then we want to make sure not to insert again
  item._idleTimeout = -1;
};


// Make a regular object able to act as a timer by setting some properties.
// This function does not start the timer, see `active()`.
// Using existing objects as timers slightly reduces object overhead.
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

  active(timer);

  return timer;
};


const clearTimeout = exports.clearTimeout = function(timer) {
  if (timer && (timer[kOnTimeout] || timer._onTimeout)) {
    timer[kOnTimeout] = timer._onTimeout = null;
    if (timer instanceof Timeout) {
      timer.close(); // for after === 0
    } else {
      unenroll(timer);
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
  active(timer);

  return timer;

  function wrapper() {
    timer._repeat();

    // Timer might be closed - no point in restarting it
    if (!timer._repeat)
      return;

    // If timer is unref'd (or was - it's permanently removed from the list.)
    if (this._handle) {
      this._handle.start(repeat);
    } else {
      timer._idleTimeout = repeat;
      active(timer);
    }
  }
};


exports.clearInterval = function(timer) {
  if (timer && timer._repeat) {
    timer._repeat = null;
    clearTimeout(timer);
  }
};


function Timeout(after) {
  this._called = false;
  this._idleTimeout = after;
  this._idlePrev = this;
  this._idleNext = this;
  this._idleStart = null;
  this._onTimeout = null;
  this._repeat = null;
}


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
      unenroll(this);
      return;
    }

    var handle = reuse(this);

    this._handle = handle || new TimerWrap();
    this._handle.owner = this;
    this._handle[kOnTimeout] = unrefdHandle;
    this._handle.start(delay);
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
    unenroll(this);
  }
  return this;
};


var immediateQueue = L.create();


function processImmediate() {
  const queue = immediateQueue;
  var domain, immediate;

  immediateQueue = L.create();

  while (L.isEmpty(queue) === false) {
    immediate = L.shift(queue);
    domain = immediate.domain;

    if (!immediate._onImmediate)
      continue;

    if (domain)
      domain.enter();

    immediate._callback = immediate._onImmediate;
    tryOnImmediate(immediate, queue);

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


// An optimization so that the try/finally only de-optimizes (since at least v8
// 4.7) what is in this smaller function.
function tryOnImmediate(immediate, queue) {
  var threw = true;
  try {
    // make the actual call outside the try/catch to allow it to be optimized
    runCallback(immediate);
    threw = false;
  } finally {
    if (threw && !L.isEmpty(queue)) {
      // Handle any remaining on next tick, assuming we're still alive to do so.
      while (!L.isEmpty(immediateQueue)) {
        L.append(queue, L.shift(immediateQueue));
      }
      immediateQueue = queue;
      process.nextTick(processImmediate);
    }
  }
}

function runCallback(timer) {
  const argv = timer._argv;
  const argc = argv ? argv.length : 0;
  switch (argc) {
    // fast-path callbacks with 0-3 arguments
    case 0:
      return timer._callback();
    case 1:
      return timer._callback(argv[0]);
    case 2:
      return timer._callback(argv[0], argv[1]);
    case 3:
      return timer._callback(argv[0], argv[1], argv[2]);
    // more than 3 arguments run slower with .apply
    default:
      return timer._callback.apply(timer, argv);
  }
}


function Immediate() {
  // assigning the callback here can cause optimize/deoptimize thrashing
  // so have caller annotate the object (node v6.0.0, v8 5.0.71.35)
  this._idleNext = null;
  this._idlePrev = null;
  this._callback = null;
  this._argv = null;
  this._onImmediate = null;
  this.domain = process.domain;
}

exports.setImmediate = function(callback, arg1, arg2, arg3) {
  if (typeof callback !== 'function') {
    throw new TypeError('"callback" argument must be a function');
  }

  var i, args;

  switch (arguments.length) {
    // fast cases
    case 0:
    case 1:
      break;
    case 2:
      args = [arg1];
      break;
    case 3:
      args = [arg1, arg2];
      break;
    case 4:
      args = [arg1, arg2, arg3];
      break;
    // slow case
    default:
      args = [arg1, arg2, arg3];
      for (i = 4; i < arguments.length; i++)
        // extend array dynamically, makes .apply run much faster in v6.0.0
        args[i - 1] = arguments[i];
      break;
  }
  // declaring it `const immediate` causes v6.0.0 to deoptimize this function
  var immediate = new Immediate();
  immediate._callback = callback;
  immediate._argv = args;
  immediate._onImmediate = callback;

  if (!process._needImmediateCallback) {
    process._needImmediateCallback = true;
    process._immediateCallback = processImmediate;
  }

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
