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
    this.length = 0;
  }

  push(v) {
    const entry = { data: v, next: null };
    if (this.length > 0)
      this.tail.next = entry;
    else
      this.head = entry;
    this.tail = entry;
    ++this.length;
  }

  shift() {
    if (this.length === 0)
      return;
    const ret = this.head.data;
    if (this.length === 1)
      this.head = this.tail = null;
    else
      this.head = this.head.next;
    --this.length;
    return ret;
  }

  clear() {
    this.head = null;
    this.tail = null;
    this.length = 0;
  }
}

function setupNextTick() {
  const async_wrap = process.binding('async_wrap');
  const async_hooks = require('async_hooks');
  const promises = require('internal/process/promises');
  const errors = require('internal/errors');
  const emitPendingUnhandledRejections = promises.setup(scheduleMicrotasks);
  const initTriggerId = async_hooks.initTriggerId;
  // Two arrays that share state between C++ and JS.
  const { async_hook_fields, async_uid_fields } = async_wrap;
  // Used to change the state of the async id stack.
  const { emitInit, emitBefore, emitAfter, emitDestroy } = async_hooks;
  // Grab the constants necessary for working with internal arrays.
  const { kInit, kDestroy, kAsyncUidCntr } = async_wrap.constants;
  const { async_id_symbol, trigger_id_symbol } = async_wrap;
  var nextTickQueue = new NextTickQueue();
  var microtasksScheduled = false;

  // Used to run V8's micro task queue.
  var _runMicrotasks = {};

  // *Must* match Environment::TickInfo::Fields in src/env.h.
  var kIndex = 0;
  var kLength = 1;

  process.nextTick = nextTick;
  // Needs to be accessible from beyond this scope.
  process._tickCallback = _tickCallback;
  process._tickDomainCallback = _tickDomainCallback;

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
        tickInfo[kLength] = nextTickQueue.length;
      }
    }
    tickInfo[kIndex] = 0;
  }

  const microTasksTickObject = {
    callback: runMicrotasksCallback,
    args: undefined,
    domain: null,
    [async_id_symbol]: 0,
    [trigger_id_symbol]: 0
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

  function _combinedTickCallback(args, callback) {
    if (args === undefined) {
      callback();
    } else {
      switch (args.length) {
        case 1:
          callback(args[0]);
          break;
        case 2:
          callback(args[0], args[1]);
          break;
        case 3:
          callback(args[0], args[1], args[2]);
          break;
        default:
          callback.apply(null, args);
      }
    }
  }

  // Run callbacks that have no domain.
  // Using domains will cause this to be overridden.
  function _tickCallback() {
    do {
      while (tickInfo[kIndex] < tickInfo[kLength]) {
        ++tickInfo[kIndex];
        const tock = nextTickQueue.shift();
        const callback = tock.callback;
        const args = tock.args;

        // CHECK(Number.isSafeInteger(tock[async_id_symbol]))
        // CHECK(tock[async_id_symbol] > 0)
        // CHECK(Number.isSafeInteger(tock[trigger_id_symbol]))
        // CHECK(tock[trigger_id_symbol] > 0)

        emitBefore(tock[async_id_symbol], tock[trigger_id_symbol]);
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
          emitDestroy(tock[async_id_symbol]);

        // Using separate callback execution functions allows direct
        // callback invocation with small numbers of arguments to avoid the
        // performance hit associated with using `fn.apply()`
        _combinedTickCallback(args, callback);

        emitAfter(tock[async_id_symbol]);

        if (kMaxCallbacksPerLoop < tickInfo[kIndex])
          tickDone();
      }
      tickDone();
      _runMicrotasks();
      emitPendingUnhandledRejections();
    } while (tickInfo[kLength] !== 0);
  }

  function _tickDomainCallback() {
    do {
      while (tickInfo[kIndex] < tickInfo[kLength]) {
        ++tickInfo[kIndex];
        const tock = nextTickQueue.shift();
        const callback = tock.callback;
        const domain = tock.domain;
        const args = tock.args;
        if (domain)
          domain.enter();

        // CHECK(Number.isSafeInteger(tock[async_id_symbol]))
        // CHECK(tock[async_id_symbol] > 0)
        // CHECK(Number.isSafeInteger(tock[trigger_id_symbol]))
        // CHECK(tock[trigger_id_symbol] > 0)

        emitBefore(tock[async_id_symbol], tock[trigger_id_symbol]);
        // TODO(trevnorris): See comment in _tickCallback() as to why this
        // isn't a good solution.
        if (async_hook_fields[kDestroy] > 0)
          emitDestroy(tock[async_id_symbol]);

        // Using separate callback execution functions allows direct
        // callback invocation with small numbers of arguments to avoid the
        // performance hit associated with using `fn.apply()`
        _combinedTickCallback(args, callback);

        emitAfter(tock[async_id_symbol]);

        if (kMaxCallbacksPerLoop < tickInfo[kIndex])
          tickDone();
        if (domain)
          domain.exit();
      }
      tickDone();
      _runMicrotasks();
      emitPendingUnhandledRejections();
    } while (tickInfo[kLength] !== 0);
  }

  class TickObject {
    constructor(callback, args, asyncId, triggerAsyncId) {
      this.callback = callback;
      this.args = args;
      this.domain = process.domain || null;
      this[async_id_symbol] = asyncId;
      this[trigger_id_symbol] = triggerAsyncId;
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

    const asyncId = ++async_uid_fields[kAsyncUidCntr];
    const triggerAsyncId = initTriggerId();
    const obj = new TickObject(callback, args, asyncId, triggerAsyncId);
    nextTickQueue.push(obj);
    ++tickInfo[kLength];
    if (async_hook_fields[kInit] > 0)
      emitInit(asyncId, 'TickObject', triggerAsyncId, obj);
  }

  // `internalNextTick()` will not enqueue any callback when the process is
  // about to exit since the callback would not have a chance to be executed.
  function internalNextTick(triggerAsyncId, callback) {
    if (typeof callback !== 'function')
      throw new TypeError('callback is not a function');
    // CHECK(Number.isSafeInteger(triggerAsyncId) || triggerAsyncId === null)
    // CHECK(triggerAsyncId > 0 || triggerAsyncId === null)

    if (process._exiting)
      return;

    if (!Number.isSafeInteger(triggerAsyncId) || triggerAsyncId <= 0) {
      triggerAsyncId = async_hooks.initTriggerId();
    }

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

    const asyncId = ++async_uid_fields[kAsyncUidCntr];
    const obj = new TickObject(callback, args, asyncId, triggerAsyncId);
    nextTickQueue.push(obj);
    ++tickInfo[kLength];
    if (async_hook_fields[kInit] > 0)
      emitInit(asyncId, 'TickObject', triggerAsyncId, obj);
  }
}
