'use strict';

// This value is used to prevent the nextTickQueue from becoming too
// large and cause the process to run out of memory. When this value
// is reached the nextTimeQueue array will be shortened (see tickDone
// for details).
const kMaxCallbacksPerLoop = 1e4;

exports.setup = setupNextTick;
// Will be overwritten when setupNextTick() is called.
exports.nextTick = null;

class NextTickQueue {
  constructor() {
    this.head = null;
    this.tail = null;
  }

  push(v) {
    const entry = { data: v, next: null };
    if (this.tail !== null)
      this.tail.next = entry;
    else
      this.head = entry;
    this.tail = entry;
  }

  shift() {
    if (this.head === null)
      return;
    const ret = this.head.data;
    if (this.head === this.tail)
      this.head = this.tail = null;
    else
      this.head = this.head.next;
    return ret;
  }

  clear() {
    this.head = null;
    this.tail = null;
  }
}

function setupNextTick() {
  const async_wrap = process.binding('async_wrap');
  const async_hooks = require('internal/async_hooks');
  const promises = require('internal/process/promises');
  const errors = require('internal/errors');
  const emitPendingUnhandledRejections = promises.setup(scheduleMicrotasks);
  const getDefaultTriggerAsyncId = async_hooks.getDefaultTriggerAsyncId;
  // Two arrays that share state between C++ and JS.
  const { async_hook_fields, async_id_fields } = async_wrap;
  // Used to change the state of the async id stack.
  const { emitInit, emitBefore, emitAfter, emitDestroy } = async_hooks;
  // Grab the constants necessary for working with internal arrays.
  const { kInit, kDestroy, kAsyncIdCounter } = async_wrap.constants;
  const { async_id_symbol, trigger_async_id_symbol } = async_wrap;
  const nextTickQueue = new NextTickQueue();
  var microtasksScheduled = false;

  // Used to run V8's micro task queue.
  var _runMicrotasks = {};

  // *Must* match Environment::TickInfo::Fields in src/env.h.
  var kIndex = 0;
  var kLength = 1;

  process.nextTick = nextTick;
  // Needs to be accessible from beyond this scope.
  process._tickCallback = _tickCallback;

  // Set the nextTick() function for internal usage.
  exports.nextTick = internalNextTick;

  // This tickInfo thing is used so that the C++ code in src/node.cc
  // can have easy access to our nextTick state, and avoid unnecessary
  // calls into JS land.
  const tickInfo = process._setupNextTick(_tickCallback, _runMicrotasks);

  _runMicrotasks = _runMicrotasks.runMicrotasks;

  function tickDone() {
    if (tickInfo[kLength] !== 0) {
      if (tickInfo[kLength] <= tickInfo[kIndex]) {
        nextTickQueue.clear();
        tickInfo[kLength] = 0;
      } else {
        tickInfo[kLength] -= tickInfo[kIndex];
      }
    }
    tickInfo[kIndex] = 0;
  }

  const microTasksTickObject = {
    callback: runMicrotasksCallback,
    args: undefined,
    [async_id_symbol]: 0,
    [trigger_async_id_symbol]: 0
  };
  function scheduleMicrotasks() {
    if (microtasksScheduled)
      return;

    // For the moment all microtasks come from the void until the PromiseHook
    // API is implemented.
    nextTickQueue.push(microTasksTickObject);

    tickInfo[kLength]++;
    microtasksScheduled = true;
  }

  function runMicrotasksCallback() {
    microtasksScheduled = false;
    _runMicrotasks();

    if (tickInfo[kIndex] < tickInfo[kLength] ||
        emitPendingUnhandledRejections()) {
      scheduleMicrotasks();
    }
  }

  function _tickCallback() {
    do {
      while (tickInfo[kIndex] < tickInfo[kLength]) {
        ++tickInfo[kIndex];
        const tock = nextTickQueue.shift();

        // CHECK(Number.isSafeInteger(tock[async_id_symbol]))
        // CHECK(tock[async_id_symbol] > 0)
        // CHECK(Number.isSafeInteger(tock[trigger_async_id_symbol]))
        // CHECK(tock[trigger_async_id_symbol] > 0)

        const asyncId = tock[async_id_symbol];
        emitBefore(asyncId, tock[trigger_async_id_symbol]);
        // emitDestroy() places the async_id_symbol into an asynchronous queue
        // that calls the destroy callback in the future. It's called before
        // calling tock.callback so destroy will be called even if the callback
        // throws an exception that is handles by 'uncaughtException' or a
        // domain.
        // TODO(trevnorris): This is a bit of a hack. It relies on the fact
        // that nextTick() doesn't allow the event loop to proceed, but if
        // any async hooks are enabled during the callback's execution then
        // this tock's after hook will be called, but not its destroy hook.
        if (async_hook_fields[kDestroy] > 0)
          emitDestroy(asyncId);

        const callback = tock.callback;
        if (tock.args === undefined)
          callback();
        else
          Reflect.apply(callback, undefined, tock.args);

        emitAfter(asyncId);

        if (kMaxCallbacksPerLoop < tickInfo[kIndex])
          tickDone();
      }
      tickDone();
      _runMicrotasks();
      emitPendingUnhandledRejections();
    } while (tickInfo[kLength] !== 0);
  }

  class TickObject {
    constructor(callback, args, asyncId, triggerAsyncId) {
      // this must be set to null first to avoid function tracking
      // on the hidden class, revisit in V8 versions after 6.2
      this.callback = null;
      this.callback = callback;
      this.args = args;

      this[async_id_symbol] = asyncId;
      this[trigger_async_id_symbol] = triggerAsyncId;

      if (async_hook_fields[kInit] > 0) {
        emitInit(asyncId,
                 'TickObject',
                 triggerAsyncId,
                 this);
      }
    }
  }

  // `nextTick()` will not enqueue any callback when the process is about to
  // exit since the callback would not have a chance to be executed.
  function nextTick(callback) {
    if (typeof callback !== 'function')
      throw new errors.TypeError('ERR_INVALID_CALLBACK');

    if (process._exiting)
      return;

    var args;
    switch (arguments.length) {
      case 1: break;
      case 2: args = [arguments[1]]; break;
      case 3: args = [arguments[1], arguments[2]]; break;
      case 4: args = [arguments[1], arguments[2], arguments[3]]; break;
      default:
        args = new Array(arguments.length - 1);
        for (var i = 1; i < arguments.length; i++)
          args[i - 1] = arguments[i];
    }

    // In V8 6.2, moving tickInfo & async_id_fields[kAsyncIdCounter] into the
    // TickObject incurs a significant performance penalty in the
    // next-tick-breadth-args benchmark (revisit later)
    ++tickInfo[kLength];
    nextTickQueue.push(new TickObject(callback,
                                      args,
                                      ++async_id_fields[kAsyncIdCounter],
                                      getDefaultTriggerAsyncId()));
  }

  // `internalNextTick()` will not enqueue any callback when the process is
  // about to exit since the callback would not have a chance to be executed.
  function internalNextTick(triggerAsyncId, callback) {
    if (typeof callback !== 'function')
      throw new errors.TypeError('ERR_INVALID_CALLBACK');
    // CHECK(Number.isSafeInteger(triggerAsyncId) || triggerAsyncId === null)
    // CHECK(triggerAsyncId > 0 || triggerAsyncId === null)

    if (process._exiting)
      return;

    var args;
    switch (arguments.length) {
      case 2: break;
      case 3: args = [arguments[2]]; break;
      case 4: args = [arguments[2], arguments[3]]; break;
      case 5: args = [arguments[2], arguments[3], arguments[4]]; break;
      default:
        args = new Array(arguments.length - 2);
        for (var i = 2; i < arguments.length; i++)
          args[i - 2] = arguments[i];
    }

    if (triggerAsyncId === null)
      triggerAsyncId = getDefaultTriggerAsyncId();
    // In V8 6.2, moving tickInfo & async_id_fields[kAsyncIdCounter] into the
    // TickObject incurs a significant performance penalty in the
    // next-tick-breadth-args benchmark (revisit later)
    ++tickInfo[kLength];
    nextTickQueue.push(new TickObject(callback,
                                      args,
                                      ++async_id_fields[kAsyncIdCounter],
                                      triggerAsyncId));
  }
}
