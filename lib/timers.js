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
  MathTrunc,
  Promise,
} = primordials;

const {
  immediateInfo,
  toggleImmediateRef
} = internalBinding('timers');
const L = require('internal/linkedlist');
const {
  async_id_symbol,
  Timeout,
  decRefCount,
  immediateInfoFields: {
    kCount,
    kRefCount
  },
  kRefed,
  initAsyncResource,
  getTimerDuration,
  timerListMap,
  timerListQueue,
  immediateQueue,
  active,
  unrefActive,
  insert
} = require('internal/timers');
const {
  promisify: { custom: customPromisify },
  deprecate
} = require('internal/util');
const debug = require('internal/util/debuglog').debuglog('timer');
const { validateCallback } = require('internal/validators');

const {
  destroyHooksExist,
  // The needed emit*() functions.
  emitDestroy
} = require('internal/async_hooks');

// Remove a timer. Cancels the timeout and resets the relevant timer properties.
function unenroll(item) {
  if (item._destroyed)
    return;

  item._destroyed = true;

  // Fewer checks may be possible, but these cover everything.
  if (destroyHooksExist() && item[async_id_symbol] !== undefined)
    emitDestroy(item[async_id_symbol]);

  L.remove(item);

  // We only delete refed lists because unrefed ones are incredibly likely
  // to come from http and be recreated shortly after.
  // TODO: Long-term this could instead be handled by creating an internal
  // clearTimeout that makes it clear that the list should not be deleted.
  // That function could then be used by http and other similar modules.
  if (item[kRefed]) {
    // Compliment truncation during insert().
    const msecs = MathTrunc(item._idleTimeout);
    const list = timerListMap[msecs];
    if (list !== undefined && L.isEmpty(list)) {
      debug('unenroll: list empty');
      timerListQueue.removeAt(list.priorityQueuePosition);
      delete timerListMap[list.msecs];
    }

    decRefCount();
  }

  // If active is called later, then we want to make sure not to insert again
  item._idleTimeout = -1;
}

// Make a regular object able to act as a timer by setting some properties.
// This function does not start the timer, see `active()`.
// Using existing objects as timers slightly reduces object overhead.
function enroll(item, msecs) {
  msecs = getTimerDuration(msecs, 'msecs');

  // If this item was already in a list somewhere
  // then we should unenroll it from that
  if (item._idleNext) unenroll(item);

  L.init(item);
  item._idleTimeout = msecs;
}


/*
 * DOM-style timers
 */


function setTimeout(callback, after, arg1, arg2, arg3) {
  validateCallback(callback);

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
        // Extend array dynamically, makes .apply run much faster in v6.0.0
        args[i - 2] = arguments[i];
      }
      break;
  }

  const timeout = new Timeout(callback, after, args, false, true);
  insert(timeout, timeout._idleTimeout);

  return timeout;
}

setTimeout[customPromisify] = function(after, value) {
  const args = value !== undefined ? [value] : value;
  return new Promise((resolve) => {
    const timeout = new Timeout(resolve, after, args, false, true);
    insert(timeout, timeout._idleTimeout);
  });
};

function clearTimeout(timer) {
  if (timer && timer._onTimeout) {
    timer._onTimeout = null;
    unenroll(timer);
  }
}

function setInterval(callback, repeat, arg1, arg2, arg3) {
  validateCallback(callback);

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
        // Extend array dynamically, makes .apply run much faster in v6.0.0
        args[i - 2] = arguments[i];
      }
      break;
  }

  const timeout = new Timeout(callback, repeat, args, true, true);
  insert(timeout, timeout._idleTimeout);

  return timeout;
}

function clearInterval(timer) {
  // clearTimeout and clearInterval can be used to clear timers created from
  // both setTimeout and setInterval, as specified by HTML Living Standard:
  // https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-setinterval
  clearTimeout(timer);
}

Timeout.prototype.close = function() {
  clearTimeout(this);
  return this;
};

const Immediate = class Immediate {
  constructor(callback, args) {
    this._idleNext = null;
    this._idlePrev = null;
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
  validateCallback(callback);

  let i, args;
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
        // Extend array dynamically, makes .apply run much faster in v6.0.0
        args[i - 1] = arguments[i];
      }
      break;
  }

  return new Immediate(callback, args);
}

setImmediate[customPromisify] = function(value) {
  return new Promise((resolve) => new Immediate(resolve, [value]));
};

function clearImmediate(immediate) {
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
}

module.exports = {
  setTimeout,
  clearTimeout,
  setImmediate,
  clearImmediate,
  setInterval,
  clearInterval,
  _unrefActive: deprecate(
    unrefActive,
    'timers._unrefActive() is deprecated.' +
    ' Please use timeout.refresh() instead.',
    'DEP0127'),
  active: deprecate(
    active,
    'timers.active() is deprecated. Please use timeout.refresh() instead.',
    'DEP0126'),
  unenroll: deprecate(
    unenroll,
    'timers.unenroll() is deprecated. Please use clearTimeout instead.',
    'DEP0096'),
  enroll: deprecate(
    enroll,
    'timers.enroll() is deprecated. Please use setTimeout instead.',
    'DEP0095')
};
