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

const {
  Timer: TimerWrap,
  setupTimers,
} = process.binding('timer_wrap');
const L = require('internal/linkedlist');
const PriorityQueue = require('internal/priority_queue');
const {
  async_id_symbol,
  trigger_async_id_symbol,
  Timeout,
  kRefed,
  initAsyncResource,
  validateTimerDuration
} = require('internal/timers');
const internalUtil = require('internal/util');
const { createPromise, promiseResolve } = process.binding('util');
const util = require('util');
const { ERR_INVALID_CALLBACK } = require('internal/errors').codes;
const debug = util.debuglog('timer');
const {
  destroyHooksExist,
  // The needed emit*() functions.
  emitBefore,
  emitAfter,
  emitDestroy
} = require('internal/async_hooks');

// *Must* match Environment::ImmediateInfo::Fields in src/env.h.
const kCount = 0;
const kRefCount = 1;
const kHasOutstanding = 2;

const [immediateInfo, toggleImmediateRef] =
  setupTimers(processImmediate, processTimers);

// HOW and WHY the timers implementation works the way it does.
//
// Timers are crucial to Node.js. Internally, any TCP I/O connection creates a
// timer so that we can time out of connections. Additionally, many user
// libraries and applications also use timers. As such there may be a
// significantly large amount of timeouts scheduled at any given time.
// Therefore, it is very important that the timers implementation is performant
// and efficient.
//
// Note: It is suggested you first read through the lib/internal/linkedlist.js
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
//
/* eslint-disable node-core/non-ascii-character */
//
// ╔════ > Object Map
// ║
// ╠══
// ║ lists: { '40': { }, '320': { etc } } (keys of millisecond duration)
// ╚══          ┌────┘
//              │
// ╔══          │
// ║ TimersList { _idleNext: { }, _idlePrev: (self) }
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
/* eslint-enable node-core/non-ascii-character */
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
// The PriorityQueue — an efficient binary heap implementation that does all
// operations in worst-case O(log n) time — which manages the order of expiring
// Timeout lists and the object map lookup of a specific list by the duration of
// timers within (or creation of a new list). However, these operations combined
// have shown to be trivial in comparison to other timers architectures.


// Object map containing linked lists of timers, keyed and sorted by their
// duration in milliseconds.
//
// - key = time in milliseconds
// - value = linked list
const lists = Object.create(null);

// This is a priority queue with a custom sorting function that first compares
// the expiry times of two lists and if they're the same then compares their
// individual IDs to determine which list was created first.
const queue = new PriorityQueue(compareTimersLists, setPosition);

function compareTimersLists(a, b) {
  const expiryDiff = a.expiry - b.expiry;
  if (expiryDiff === 0) {
    if (a.id < b.id)
      return -1;
    if (a.id > b.id)
      return 1;
  }
  return expiryDiff;
}

function setPosition(node, pos) {
  node.priorityQueuePosition = pos;
}

let handle = null;
let nextExpiry = Infinity;

let timerListId = Number.MIN_SAFE_INTEGER;
let refCount = 0;

function incRefCount() {
  if (refCount++ === 0)
    handle.ref();
}

function decRefCount() {
  if (--refCount === 0)
    handle.unref();
}

function createHandle(refed) {
  debug('initial run, creating TimerWrap handle');
  handle = new TimerWrap();
  if (!refed)
    handle.unref();
}

// Schedule or re-schedule a timer.
// The item must have been enroll()'d first.
const active = exports.active = function(item) {
  insert(item, true, TimerWrap.now());
};

// Internal APIs that need timeouts should use `_unrefActive()` instead of
// `active()` so that they do not unnecessarily keep the process open.
exports._unrefActive = function(item) {
  insert(item, false, TimerWrap.now());
};


// The underlying logic for scheduling or re-scheduling a timer.
//
// Appends a timer onto the end of an existing timers list, or creates a new
// TimerWrap backed list if one does not already exist for the specified timeout
// duration.
function insert(item, refed, start) {
  const msecs = item._idleTimeout;
  if (msecs < 0 || msecs === undefined)
    return;

  item._idleStart = start;

  // Use an existing list if there is one, otherwise we need to make a new one.
  var list = lists[msecs];
  if (list === undefined) {
    debug('no %d list was found in insert, creating a new one', msecs);
    const expiry = start + msecs;
    lists[msecs] = list = new TimersList(expiry, msecs);
    queue.insert(list);

    if (nextExpiry > expiry) {
      if (handle === null)
        createHandle(refed);
      handle.start(msecs);
      nextExpiry = expiry;
    }
  }

  if (!item[async_id_symbol] || item._destroyed) {
    item._destroyed = false;
    initAsyncResource(item, 'Timeout');
  }

  if (refed === !item[kRefed]) {
    if (refed)
      incRefCount();
    else
      decRefCount();
  }
  item[kRefed] = refed;

  L.append(list, item);
}

function TimersList(expiry, msecs) {
  this._idleNext = this; // Create the list with the linkedlist properties to
  this._idlePrev = this; // prevent any unnecessary hidden class changes.
  this.expiry = expiry;
  this.id = timerListId++;
  this.msecs = msecs;
  this.priorityQueuePosition = null;
}

const { _tickCallback: runNextTicks } = process;
function processTimers(now) {
  debug('process timer lists %d', now);
  nextExpiry = Infinity;

  let list, ran;
  while (list = queue.peek()) {
    if (list.expiry > now)
      break;
    if (ran)
      runNextTicks();
    listOnTimeout(list, now);
    ran = true;
  }

  if (refCount > 0)
    handle.ref();
  else
    handle.unref();

  if (list !== undefined) {
    nextExpiry = list.expiry;
    handle.start(Math.max(nextExpiry - TimerWrap.now(), 1));
  }

  return true;
}

function listOnTimeout(list, now) {
  const msecs = list.msecs;

  debug('timeout callback %d', msecs);
  debug('now: %d', now);

  var diff, timer;
  while (timer = L.peek(list)) {
    diff = now - timer._idleStart;

    // Check if this loop iteration is too early for the next timer.
    // This happens if there are more timers scheduled for later in the list.
    if (diff < msecs) {
      list.expiry = timer._idleStart + msecs;
      list.id = timerListId++;
      queue.percolateDown(1);
      debug('%d list wait because diff is %d', msecs, diff);
      return;
    }

    // The actual logic for when a timeout happens.
    L.remove(timer);

    const asyncId = timer[async_id_symbol];

    if (!timer._onTimeout) {
      if (timer[kRefed])
        refCount--;
      timer[kRefed] = null;

      if (destroyHooksExist() && !timer._destroyed) {
        emitDestroy(asyncId);
        timer._destroyed = true;
      }
      continue;
    }

    emitBefore(asyncId, timer[trigger_async_id_symbol]);

    tryOnTimeout(timer);

    emitAfter(asyncId);
  }

  // If `L.peek(list)` returned nothing, the list was either empty or we have
  // called all of the timer timeouts.
  // As such, we can remove the list from the object map and the PriorityQueue.
  debug('%d list empty', msecs);

  // The current list may have been removed and recreated since the reference
  // to `list` was created. Make sure they're the same instance of the list
  // before destroying.
  if (list === lists[msecs]) {
    delete lists[msecs];
    queue.shift();
  }
}


// An optimization so that the try/finally only de-optimizes (since at least v8
// 4.7) what is in this smaller function.
function tryOnTimeout(timer, start) {
  if (start === undefined && timer._repeat)
    start = TimerWrap.now();
  try {
    ontimeout(timer);
  } finally {
    if (timer._repeat) {
      rearm(timer, start);
    } else {
      if (timer[kRefed])
        refCount--;
      timer[kRefed] = null;

      if (destroyHooksExist() && !timer._destroyed) {
        emitDestroy(timer[async_id_symbol]);
        timer._destroyed = true;
      }
    }
  }
}


// Remove a timer. Cancels the timeout and resets the relevant timer properties.
function unenroll(item) {
  // Fewer checks may be possible, but these cover everything.
  if (destroyHooksExist() &&
      item[async_id_symbol] !== undefined &&
      !item._destroyed) {
    emitDestroy(item[async_id_symbol]);
    item._destroyed = true;
  }

  L.remove(item);

  // We only delete refed lists because unrefed ones are incredibly likely
  // to come from http and be recreated shortly after.
  // TODO: Long-term this could instead be handled by creating an internal
  // clearTimeout that makes it clear that the list should not be deleted.
  // That function could then be used by http and other similar modules.
  if (item[kRefed]) {
    const list = lists[item._idleTimeout];
    if (list !== undefined && L.isEmpty(list)) {
      debug('unenroll: list empty');
      queue.removeAt(list.priorityQueuePosition);
      delete lists[list.msecs];
    }

    decRefCount();
  }
  item[kRefed] = null;

  // if active is called later, then we want to make sure not to insert again
  item._idleTimeout = -1;
}

exports.unenroll = util.deprecate(unenroll,
                                  'timers.unenroll() is deprecated. ' +
                                  'Please use clearTimeout instead.',
                                  'DEP0096');


// Make a regular object able to act as a timer by setting some properties.
// This function does not start the timer, see `active()`.
// Using existing objects as timers slightly reduces object overhead.
function enroll(item, msecs) {
  msecs = validateTimerDuration(msecs);

  // if this item was already in a list somewhere
  // then we should unenroll it from that
  if (item._idleNext) unenroll(item);

  L.init(item);
  item._idleTimeout = msecs;
}

exports.enroll = util.deprecate(enroll,
                                'timers.enroll() is deprecated. ' +
                                'Please use setTimeout instead.',
                                'DEP0095');


/*
 * DOM-style timers
 */


function setTimeout(callback, after, arg1, arg2, arg3) {
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK();
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

  const timeout = new Timeout(callback, after, args, false);
  active(timeout);

  return timeout;
}

setTimeout[internalUtil.promisify.custom] = function(after, value) {
  const promise = createPromise();
  const timeout = new Timeout(promise, after, [value], false);
  active(timeout);

  return promise;
};

exports.setTimeout = setTimeout;


function ontimeout(timer) {
  const args = timer._timerArgs;
  if (typeof timer._onTimeout !== 'function')
    return promiseResolve(timer._onTimeout, args[0]);
  if (!args)
    timer._onTimeout();
  else
    Reflect.apply(timer._onTimeout, timer, args);
}

function rearm(timer, start) {
  // // Do not re-arm unenroll'd or closed timers.
  if (timer._idleTimeout === -1)
    return;

  timer._idleTimeout = timer._repeat;
  insert(timer, timer[kRefed], start);
}


const clearTimeout = exports.clearTimeout = function clearTimeout(timer) {
  if (timer && timer._onTimeout) {
    timer._onTimeout = null;
    unenroll(timer);
  }
};


exports.setInterval = function setInterval(callback, repeat, arg1, arg2, arg3) {
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK();
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

  const timeout = new Timeout(callback, repeat, args, true);
  active(timeout);

  return timeout;
};

exports.clearInterval = function clearInterval(timer) {
  // clearTimeout and clearInterval can be used to clear timers created from
  // both setTimeout and setInterval, as specified by HTML Living Standard:
  // https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-setinterval
  clearTimeout(timer);
};


Timeout.prototype.unref = function() {
  if (this[kRefed]) {
    this[kRefed] = false;
    decRefCount();
  }
  return this;
};

Timeout.prototype.ref = function() {
  if (this[kRefed] === false) {
    this[kRefed] = true;
    incRefCount();
  }
  return this;
};

Timeout.prototype.hasRef = function() {
  return !!this[kRefed];
};

Timeout.prototype.close = function() {
  clearTimeout(this);
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
    immediate[kRefed] = null;

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

    if (destroyHooksExist()) {
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

    initAsyncResource(this, 'Immediate');

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

  hasRef() {
    return !!this[kRefed];
  }
};

function setImmediate(callback, arg1, arg2, arg3) {
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK();
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


exports.clearImmediate = function clearImmediate(immediate) {
  if (!immediate || immediate._destroyed)
    return;

  immediateInfo[kCount]--;
  immediate._destroyed = true;

  if (immediate[kRefed] && --immediateInfo[kRefCount] === 0)
    toggleImmediateRef(false);
  immediate[kRefed] = null;

  if (destroyHooksExist()) {
    emitDestroy(immediate[async_id_symbol]);
  }

  immediate._onImmediate = null;

  immediateQueue.remove(immediate);
};
