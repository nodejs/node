'use strict';

// This value is used to prevent the nextTickQueue from becoming too
// large and cause the process to run out of memory. When this value
// is reached the nextTimeQueue array will be shortend (see tickDone
// for details).
const kMaxCallbacksPerLoop = 1e4;

exports.setup = setupNextTick;
// Will be overwritten when setupNextTick() is called.
exports.nextTick = null;

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
  const { pushAsyncIds, popAsyncIds } = async_wrap;
  // The needed emit*() functions.
  const { emitInit, emitBefore, emitAfter, emitDestroy } = async_hooks;
  // Grab the constants necessary for working with internal arrays.
  const { kInit, kBefore, kAfter, kDestroy, kAsyncUidCntr, kInitTriggerId } =
      async_wrap.constants;
  const { async_id_symbol, trigger_id_symbol } = async_wrap;
  var nextTickQueue = [];
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
        nextTickQueue = [];
        tickInfo[kLength] = 0;
      } else {
        nextTickQueue.splice(0, tickInfo[kIndex]);
        tickInfo[kLength] = nextTickQueue.length;
      }
    }
    tickInfo[kIndex] = 0;
  }

  function scheduleMicrotasks() {
    if (microtasksScheduled)
      return;

    const tickObject =
        new TickObject(runMicrotasksCallback, undefined, null);
    // For the moment all microtasks come from the void until the PromiseHook
    // API is implemented.
    tickObject[async_id_symbol] = 0;
    tickObject[trigger_id_symbol] = 0;
    nextTickQueue.push(tickObject);

    tickInfo[kLength]++;
    microtasksScheduled = true;
  }

  function runMicrotasksCallback() {
    microtasksScheduled = false;
    _runMicrotasks();

    if (tickInfo[kIndex] < tickInfo[kLength] ||
        emitPendingUnhandledRejections())
      scheduleMicrotasks();
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

  // TODO(trevnorris): Using std::stack of Environment::AsyncHooks::ids_stack_
  // is much slower here than was the Float64Array stack used in a previous
  // implementation. Problem is the Float64Array stack was a bit brittle.
  // Investigate how to harden that implementation and possibly reintroduce it.
  function nextTickEmitBefore(asyncId, triggerId) {
    if (async_hook_fields[kBefore] > 0)
      emitBefore(asyncId, triggerId);
    else
      pushAsyncIds(asyncId, triggerId);
  }

  function nextTickEmitAfter(asyncId) {
    if (async_hook_fields[kAfter] > 0)
      emitAfter(asyncId);
    else
      popAsyncIds(asyncId);
  }

  // Run callbacks that have no domain.
  // Using domains will cause this to be overridden.
  function _tickCallback() {
    do {
      while (tickInfo[kIndex] < tickInfo[kLength]) {
        const tock = nextTickQueue[tickInfo[kIndex]++];
        const callback = tock.callback;
        const args = tock.args;

        // CHECK(Number.isSafeInteger(tock[async_id_symbol]))
        // CHECK(tock[async_id_symbol] > 0)
        // CHECK(Number.isSafeInteger(tock[trigger_id_symbol]))
        // CHECK(tock[trigger_id_symbol] > 0)

        nextTickEmitBefore(tock[async_id_symbol], tock[trigger_id_symbol]);
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

        nextTickEmitAfter(tock[async_id_symbol]);

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
        const tock = nextTickQueue[tickInfo[kIndex]++];
        const callback = tock.callback;
        const domain = tock.domain;
        const args = tock.args;
        if (domain)
          domain.enter();

        // CHECK(Number.isSafeInteger(tock[async_id_symbol]))
        // CHECK(tock[async_id_symbol] > 0)
        // CHECK(Number.isSafeInteger(tock[trigger_id_symbol]))
        // CHECK(tock[trigger_id_symbol] > 0)

        nextTickEmitBefore(tock[async_id_symbol], tock[trigger_id_symbol]);
        // TODO(trevnorris): See comment in _tickCallback() as to why this
        // isn't a good solution.
        if (async_hook_fields[kDestroy] > 0)
          emitDestroy(tock[async_id_symbol]);

        // Using separate callback execution functions allows direct
        // callback invocation with small numbers of arguments to avoid the
        // performance hit associated with using `fn.apply()`
        _combinedTickCallback(args, callback);

        nextTickEmitAfter(tock[async_id_symbol]);

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

  function TickObject(callback, args, domain) {
    this.callback = callback;
    this.domain = domain;
    this.args = args;
    this[async_id_symbol] = -1;
    this[trigger_id_symbol] = -1;
  }

  function setupInit(tickObject, triggerId) {
    tickObject[async_id_symbol] = ++async_uid_fields[kAsyncUidCntr];
    tickObject[trigger_id_symbol] = triggerId || initTriggerId();
    if (async_hook_fields[kInit] > 0) {
      emitInit(tickObject[async_id_symbol],
               'TickObject',
               tickObject[trigger_id_symbol],
               tickObject);
    }
  }

  function nextTick(callback) {
    if (typeof callback !== 'function')
      throw new errors.TypeError('ERR_INVALID_CALLBACK');
    // on the way out, don't bother. it won't get fired anyway.
    if (process._exiting)
      return;

    var args;
    if (arguments.length > 1) {
      args = new Array(arguments.length - 1);
      for (var i = 1; i < arguments.length; i++)
        args[i - 1] = arguments[i];
    }

    var obj = new TickObject(callback, args, process.domain || null);
    setupInit(obj, null);
    nextTickQueue.push(obj);
    tickInfo[kLength]++;
  }

  function internalNextTick(triggerId, callback) {
    if (typeof callback !== 'function')
      throw new TypeError('callback is not a function');
    // CHECK(Number.isSafeInteger(triggerId) || triggerId === null)
    // CHECK(triggerId > 0 || triggerId === null)

    if (process._exiting)
      return;

    var args;
    if (arguments.length > 2) {
      args = new Array(arguments.length - 2);
      for (var i = 2; i < arguments.length; i++)
        args[i - 2] = arguments[i];
    }

    var obj = new TickObject(callback, args, process.domain || null);
    setupInit(obj, triggerId);
    // The call to initTriggerId() was skipped, so clear kInitTriggerId.
    async_uid_fields[kInitTriggerId] = 0;
    nextTickQueue.push(obj);
    tickInfo[kLength]++;
  }
}
