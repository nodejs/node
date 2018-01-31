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

'use strict';

const async_wrap = process.binding('async_wrap');
const {
  Timer: TimerWrap,
  setImmediateCallback,
} = process.binding('timer_wrap');
const L = require('internal/linkedlist');
const timerInternals = require('internal/timers');
const internalUtil = require('internal/util');
const { createPromise, promiseResolve } = process.binding('util');
const assert = require('assert');
const util = require('util');
const errors = require('internal/errors');
const debug = util.debuglog('timer');
const kOnTimeout = TimerWrap.kOnTimeout | 0;
// Two arrays that share state between C++ and JS.
const { async_hook_fields, async_id_fields } = async_wrap;
const {
  getDefaultTriggerAsyncId,
  // The needed emit*() functions.
  emitInit,
  emitBefore,
  emitAfter,
  emitDestroy
} = require('internal/async_hooks');
// Grab the constants necessary for working with internal arrays.
const { kInit, kDestroy, kAsyncIdCounter } = async_wrap.constants;
// Symbols for storing async id state.
const async_id_symbol = timerInternals.async_id_symbol;
const trigger_async_id_symbol = timerInternals.trigger_async_id_symbol;

// *Must* match Environment::ImmediateInfo::Fields in src/env.h.
const kCount = 0;
const kRefCount = 1;
const kHasOutstanding = 2;

const [immediateInfo, toggleImmediateRef] =
  setImmediateCallback(processImmediate);

const kRefed = Symbol('refed');

// The Timeout class
const Timeout = timerInternals.Timeout;


// HOW and WHY the timers implementation works the way it does.
//
// Timers are crucial to Node.js. Internally, any TCP I/O connection creates a
// timer so that we can time out of connections. Additionally, many user
// libraries and applications also use timers. As such there may be a
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
// Timeouts only need to process any timers currently due to expire, which will
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
const refedLists = Object.create(null);
const unrefedLists = Object.create(null);


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
function insert(item, unrefed, start) {
  const msecs = item._idleTimeout;
  if (msecs < 0 || msecs === undefined) return;

  if (typeof start === 'number') {
    item._idleStart = start;
  } else {
    item._idleStart = TimerWrap.now();
  }

  const lists = unrefed === true ? unrefedLists : refedLists;

  // Use an existing list if there is one, otherwise we need to make a new one.
  var list = lists[msecs];
  if (list === undefined) {
    debug('no %d list was found in insert, creating a new one', msecs);
    lists[msecs] = list = new TimersList(msecs, unrefed);
  }

  if (!item[async_id_symbol] || item._destroyed) {
    item._destroyed = false;
    item[async_id_symbol] = ++async_id_fields[kAsyncIdCounter];
    item[trigger_async_id_symbol] = getDefaultTriggerAsyncId();
    if (async_hook_fields[kInit] > 0) {
      emitInit(item[async_id_symbol],
               'Timeout',
               item[trigger_async_id_symbol],
               item);
    }
  }

  L.append(list, item);
  assert(!L.isEmpty(list)); // list is not empty
}

function TimersList(msecs, unrefed) {
  this._idleNext = this; // Create the list with the linkedlist properties to
  this._idlePrev = this; // prevent any unnecessary hidden class changes.
  this._unrefed = unrefed;
  this.msecs = msecs;
  this.nextTick = false;

  const timer = this._timer = new TimerWrap();
  timer._list = this;

  if (unrefed === true)
    timer.unref();
  timer.start(msecs);
}

// adds listOnTimeout to the C++ object prototype, as
// V8 would not inline it otherwise.
TimerWrap.prototype[kOnTimeout] = function listOnTimeout() {
  var list = this._list;
  var msecs = list.msecs;

  if (list.nextTick) {
    list.nextTick = false;
    process.nextTick(listOnTimeoutNT, list);
    return;
  }

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
        timeRemaining = 1;
      }
      this.start(timeRemaining);
      debug('%d list wait because diff is %d', msecs, diff);
      return;
    }

    // The actual logic for when a timeout happens.

    L.remove(timer);
    assert(timer !== L.peek(list));

    if (!timer._onTimeout) {
      if (async_hook_fields[kDestroy] > 0 && !timer._destroyed &&
            typeof timer[async_id_symbol] === 'number') {
        emitDestroy(timer[async_id_symbol]);
        timer._destroyed = true;
      }
      continue;
    }

    tryOnTimeout(timer, list);
  }

  // If `L.peek(list)` returned nothing, the list was either empty or we have
  // called all of the timer timeouts.
  // As such, we can remove the list and clean up the TimerWrap C++ handle.
  debug('%d list empty', msecs);
  assert(L.isEmpty(list));

  // Either refedLists[msecs] or unrefedLists[msecs] may have been removed and
  // recreated since the reference to `list` was created. Make sure they're
  // the same instance of the list before destroying.
  if (list._unrefed === true && list === unrefedLists[msecs]) {
    delete unrefedLists[msecs];
  } else if (list === refedLists[msecs]) {
    delete refedLists[msecs];
  }

  // Do not close the underlying handle if its ownership has changed
  // (e.g it was unrefed in its callback).
  if (this.owner)
    return;

  this.close();
};


// An optimization so that the try/finally only de-optimizes (since at least v8
// 4.7) what is in this smaller function.
function tryOnTimeout(timer, list) {
  timer._called = true;
  const timerAsyncId = (typeof timer[async_id_symbol] === 'number') ?
    timer[async_id_symbol] : null;
  var threw = true;
  if (timerAsyncId !== null)
    emitBefore(timerAsyncId, timer[trigger_async_id_symbol]);
  try {
    ontimeout(timer);
    threw = false;
  } finally {
    if (timerAsyncId !== null) {
      if (!threw)
        emitAfter(timerAsyncId);
      if (!timer._repeat && async_hook_fields[kDestroy] > 0 &&
          !timer._destroyed) {
        emitDestroy(timerAsyncId);
        timer._destroyed = true;
      }
    }

    if (!threw) return;

    // Postpone all later list events to next tick. We need to do this
    // so that the events are called in the order they were created.
    const lists = list._unrefed === true ? unrefedLists : refedLists;
    for (var key in lists) {
      if (key > list.msecs) {
        lists[key].nextTick = true;
      }
    }
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
  if (list !== undefined && L.isEmpty(list)) {
    debug('reuse hit');
    list._timer.stop();
    delete refedLists[item._idleTimeout];
    return list._timer;
  }

  return null;
}


// Remove a timer. Cancels the timeout and resets the relevant timer properties.
const unenroll = exports.unenroll = function(item) {
  // Fewer checks may be possible, but these cover everything.
  if (async_hook_fields[kDestroy] > 0 &&
      item &&
      typeof item[async_id_symbol] === 'number' &&
      !item._destroyed) {
    emitDestroy(item[async_id_symbol]);
    item._destroyed = true;
  }

  var handle = reuse(item);
  if (handle !== null) {
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
  item._idleTimeout = timerInternals.validateTimerDuration(msecs);

  // if this item was already in a list somewhere
  // then we should unenroll it from that
  if (item._idleNext) unenroll(item);

  L.init(item);
};


/*
 * DOM-style timers
 */


function setTimeout(callback, after, arg1, arg2, arg3) {
  if (typeof callback !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  var i, args;
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

  const timeout = new Timeout(callback, after, args, false, false);
  active(timeout);

  return timeout;
}

setTimeout[internalUtil.promisify.custom] = function(after, value) {
  const promise = createPromise();
  const timeout = new Timeout(promise, after, [value], false, false);
  active(timeout);

  return promise;
};

exports.setTimeout = setTimeout;


function ontimeout(timer) {
  var args = timer._timerArgs;
  if (typeof timer._onTimeout !== 'function')
    return promiseResolve(timer._onTimeout, args[0]);
  const start = TimerWrap.now();
  if (!args)
    timer._onTimeout();
  else
    Reflect.apply(timer._onTimeout, timer, args);
  if (timer._repeat)
    rearm(timer, start);
}


function rearm(timer, start) {
  // // Do not re-arm unenroll'd or closed timers.
  if (timer._idleTimeout === -1) return;

  // If timer is unref'd (or was - it's permanently removed from the list.)
  if (timer._handle && timer instanceof Timeout) {
    timer._handle.start(timer._repeat);
  } else {
    timer._idleTimeout = timer._repeat;

    const duration = TimerWrap.now() - start;
    if (duration >= timer._repeat) {
      // If callback duration >= timer._repeat,
      // add 1 ms to avoid blocking eventloop
      insert(timer, false, start + duration - timer._repeat + 1);
    } else {
      insert(timer, false, start);
    }
  }
}


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


exports.setInterval = function(callback, repeat, arg1, arg2, arg3) {
  if (typeof callback !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  var i, args;
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

  const timeout = new Timeout(callback, repeat, args, true, false);
  active(timeout);

  return timeout;
};

exports.clearInterval = function(timer) {
  if (timer && timer._repeat) {
    timer._repeat = null;
    clearTimeout(timer);
  }
};


function unrefdHandle() {
  // Don't attempt to call the callback if it is not a function.
  if (typeof this.owner._onTimeout === 'function') {
    ontimeout(this.owner);
  }

  // Make sure we clean up if the callback is no longer a function
  // even if the timer is an interval.
  if (!this.owner._repeat ||
      typeof this.owner._onTimeout !== 'function') {
    this.owner.close();
  }
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
    if (handle !== null) {
      handle._list = undefined;
    }

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
    if (async_hook_fields[kDestroy] > 0 &&
        typeof this[async_id_symbol] === 'number' &&
        !this._destroyed) {
      emitDestroy(this[async_id_symbol]);
      this._destroyed = true;
    }

    this._idleTimeout = -1;
    this._handle[kOnTimeout] = null;
    this._handle.close();
  } else {
    unenroll(this);
  }
  return this;
};


// A linked list for storing `setImmediate()` requests
function ImmediateList() {
  this.head = null;
  this.tail = null;
}

// Appends an item to the end of the linked list, adjusting the current tail's
// previous and next pointers where applicable
ImmediateList.prototype.append = function(item) {
  if (this.tail !== null) {
    this.tail._idleNext = item;
    item._idlePrev = this.tail;
  } else {
    this.head = item;
  }
  this.tail = item;
};

// Removes an item from the linked list, adjusting the pointers of adjacent
// items and the linked list's head or tail pointers as necessary
ImmediateList.prototype.remove = function(item) {
  if (item._idleNext !== null) {
    item._idleNext._idlePrev = item._idlePrev;
  }

  if (item._idlePrev !== null) {
    item._idlePrev._idleNext = item._idleNext;
  }

  if (item === this.head)
    this.head = item._idleNext;
  if (item === this.tail)
    this.tail = item._idlePrev;

  item._idleNext = null;
  item._idlePrev = null;
};

// Create a single linked list instance only once at startup
const immediateQueue = new ImmediateList();

// If an uncaught exception was thrown during execution of immediateQueue,
// this queue will store all remaining Immediates that need to run upon
// resolution of all error handling (if process is still alive).
const outstandingQueue = new ImmediateList();


function processImmediate() {
  const queue = outstandingQueue.head !== null ?
    outstandingQueue : immediateQueue;
  var immediate = queue.head;
  const tail = queue.tail;

  // Clear the linked list early in case new `setImmediate()` calls occur while
  // immediate callbacks are executed
  queue.head = queue.tail = null;

  let count = 0;
  let refCount = 0;

  while (immediate !== null) {
    immediate._destroyed = true;

    const asyncId = immediate[async_id_symbol];
    emitBefore(asyncId, immediate[trigger_async_id_symbol]);

    count++;
    if (immediate[kRefed])
      refCount++;
    immediate[kRefed] = undefined;

    tryOnImmediate(immediate, tail, count, refCount);

    emitAfter(asyncId);

    immediate = immediate._idleNext;
  }

  immediateInfo[kCount] -= count;
  immediateInfo[kRefCount] -= refCount;
  immediateInfo[kHasOutstanding] = 0;
}

// An optimization so that the try/finally only de-optimizes (since at least v8
// 4.7) what is in this smaller function.
function tryOnImmediate(immediate, oldTail, count, refCount) {
  var threw = true;
  try {
    // make the actual call outside the try/finally to allow it to be optimized
    runCallback(immediate);
    threw = false;
  } finally {
    immediate._onImmediate = null;

    if (async_hook_fields[kDestroy] > 0) {
      emitDestroy(immediate[async_id_symbol]);
    }

    if (threw) {
      immediateInfo[kCount] -= count;
      immediateInfo[kRefCount] -= refCount;

      if (immediate._idleNext !== null) {
        // Handle any remaining Immediates after error handling has resolved,
        // assuming we're still alive to do so.
        outstandingQueue.head = immediate._idleNext;
        outstandingQueue.tail = oldTail;
        immediateInfo[kHasOutstanding] = 1;
      }
    }
  }
}

function runCallback(timer) {
  const argv = timer._argv;
  if (typeof timer._onImmediate !== 'function')
    return promiseResolve(timer._onImmediate, argv[0]);
  if (!argv)
    return timer._onImmediate();
  Reflect.apply(timer._onImmediate, timer, argv);
}


const Immediate = class Immediate {
  constructor(callback, args) {
    this._idleNext = null;
    this._idlePrev = null;
    // this must be set to null first to avoid function tracking
    // on the hidden class, revisit in V8 versions after 6.2
    this._onImmediate = null;
    this._onImmediate = callback;
    this._argv = args;
    this._destroyed = false;
    this[kRefed] = false;

    this[async_id_symbol] = ++async_id_fields[kAsyncIdCounter];
    this[trigger_async_id_symbol] = getDefaultTriggerAsyncId();
    if (async_hook_fields[kInit] > 0) {
      emitInit(this[async_id_symbol],
               'Immediate',
               this[trigger_async_id_symbol],
               this);
    }

    this.ref();
    immediateInfo[kCount]++;

    immediateQueue.append(this);
  }

  ref() {
    if (this[kRefed] === false) {
      this[kRefed] = true;
      if (immediateInfo[kRefCount]++ === 0)
        toggleImmediateRef(true);
    }
    return this;
  }

  unref() {
    if (this[kRefed] === true) {
      this[kRefed] = false;
      if (--immediateInfo[kRefCount] === 0)
        toggleImmediateRef(false);
    }
    return this;
  }
};

function setImmediate(callback, arg1, arg2, arg3) {
  if (typeof callback !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }

  var i, args;
  switch (arguments.length) {
    // fast cases
    case 1:
      break;
    case 2:
      args = [arg1];
      break;
    case 3:
      args = [arg1, arg2];
      break;
    default:
      args = [arg1, arg2, arg3];
      for (i = 4; i < arguments.length; i++) {
        // extend array dynamically, makes .apply run much faster in v6.0.0
        args[i - 1] = arguments[i];
      }
      break;
  }

  return new Immediate(callback, args);
}

setImmediate[internalUtil.promisify.custom] = function(value) {
  const promise = createPromise();
  new Immediate(promise, [value]);
  return promise;
};

exports.setImmediate = setImmediate;


exports.clearImmediate = function(immediate) {
  if (!immediate || immediate._destroyed)
    return;

  immediateInfo[kCount]--;
  immediate._destroyed = true;

  if (immediate[kRefed] && --immediateInfo[kRefCount] === 0)
    toggleImmediateRef(false);
  immediate[kRefed] = undefined;

  if (async_hook_fields[kDestroy] > 0) {
    emitDestroy(immediate[async_id_symbol]);
  }

  immediate._onImmediate = null;

  immediateQueue.remove(immediate);
};
