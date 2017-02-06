'use strict';

exports.setup = setupNextTick;

function setupNextTick() {
  const async_wrap = process.binding('async_wrap');
  const async_hooks = require('async_hooks');
  const promises = require('internal/process/promises');
  const emitPendingUnhandledRejections = promises.setup(scheduleMicrotasks);
  const initTriggerId = async_hooks.initTriggerId;
  // Two arrays that share state between C++ and JS.
  const { async_uid_fields, async_hook_fields } = async_wrap;
  // Two functions to manipulate the id stack.
  const { genIdArray, trimIdArray } = async_wrap;
  // The needed emit*() functions.
  const { emitInit, emitBefore, emitAfter, emitDestroy } = async_hooks;
  // Grab the constants necessary for working with internal arrays.
  const { kInit, kBefore, kAfter, kDestroy, kAsyncUidCntr, kIdStackIndex,
          kIdStackSize, kIdStackLimit } = async_wrap.constants;
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
        new TickObject(runMicrotasksCallback, undefined, null, true);
    // For the moment all microtasks come from the void. Until the PromiseHook
    // API is implemented in V8.
    tickObject._asyncId = 0;
    tickObject._triggerId = 0;
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

  function nextTickEmitBefore(id, triggerId) {
    if (async_hook_fields[kBefore] > 0)
      return emitBefore(id, triggerId);

    // Do same basic operations to the id stack as emitBefore/emitAfter.
    async_hook_fields[kIdStackIndex] += 2;
    async_hook_fields[kIdStackSize] += 2;

    if (async_hook_fields[kIdStackIndex] >= kIdStackLimit) {
      genIdArray();
    }

    async_wrap.async_id_stack[async_hook_fields[kIdStackIndex]] = id;
    async_wrap.async_id_stack[async_hook_fields[kIdStackIndex] + 1] =
      triggerId === undefined ? id : triggerId;
  }

  function nextTickEmitAfter(id) {
    // CHECK(id > 1)
    if (async_hook_fields[kAfter] > 0)
      return emitAfter(id);

    async_hook_fields[kIdStackIndex] -= 2;
    async_hook_fields[kIdStackSize] -= 2;

    if (async_hook_fields[kIdStackIndex] === 0 &&
        async_hook_fields[kIdStackSize] > 0) {
      trimIdArray();
    }
  }

  // Run callbacks that have no domain.
  // Using domains will cause this to be overridden.
  function _tickCallback() {
    do {
      while (tickInfo[kIndex] < tickInfo[kLength]) {
        const tock = nextTickQueue[tickInfo[kIndex]++];
        const callback = tock.callback;
        const args = tock.args;

        // CHECK(Number.isSafeInteger(tock._asyncId) && tock._asyncId > 0)
        // CHECK(Number.isSafeInteger(tock._triggerId) && tock._triggerId > 0)

        nextTickEmitBefore(tock._asyncId, tock._triggerId);
        // TODO(trevnorris): Checking kDestroy this way is a cheat because if
        // any destroy() hooks are enabled during the callback then they won't
        // be notified of this id's destroy. Though that may be alright since
        // no other callbacks would have been called for that id.
        // Emit destroy() here because if there's an error the destroy() still
        // needs to be called for this id. Works because all destroy() are
        // called asyncronously.
        if (async_hook_fields[kDestroy] > 0)
          emitDestroy(tock._asyncId);

        // Using separate callback execution functions allows direct
        // callback invocation with small numbers of arguments to avoid the
        // performance hit associated with using `fn.apply()`
        _combinedTickCallback(args, callback);

        nextTickEmitAfter(tock._asyncId);

        if (1e4 < tickInfo[kIndex])
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

        // CHECK(Number.isSafeInteger(tock._asyncId) && tock._asyncId > 0)
        // CHECK(Number.isSafeInteger(tock._triggerId) && tock._triggerId > 0)

        nextTickEmitBefore(tock._asyncId, tock._triggerId);
        // Emit destroy() here because if there's an error the destroy() still
        // needs to be called for this id. Works because all destroy() are
        // called asyncronously.
        if (async_hook_fields[kDestroy] > 0)
          emitDestroy(tock._asyncId);

        // Using separate callback execution functions allows direct
        // callback invocation with small numbers of arguments to avoid the
        // performance hit associated with using `fn.apply()`
        _combinedTickCallback(args, callback);

        nextTickEmitAfter(tock._asyncId);

        if (1e4 < tickInfo[kIndex])
          tickDone();
        if (domain)
          domain.exit();
      }
      tickDone();
      _runMicrotasks();
      emitPendingUnhandledRejections();
    } while (tickInfo[kLength] !== 0);
  }

  function TickObject(callback, args, domain, isMicrotask) {
    this.callback = callback;
    this.domain = domain;
    this.args = args;
    this._asyncId = -1;
    this._triggerId = -1;
    if (!isMicrotask)
      setupInit(this);
  }

  // This function is for performance reasons. If the code here is placed
  // directly in TickObject it would be over the maximum character count
  // to be inlined, and it would suffer a penalty lose.
  function setupInit(self) {
    self._asyncId = ++async_uid_fields[kAsyncUidCntr];
    self._triggerId = initTriggerId();
    if (async_hook_fields[kInit] > 0)
      emitInit(self._asyncId, 'TickObject', self._triggerId, self);
  }

  function nextTick(callback) {
    if (typeof callback !== 'function')
      throw new TypeError('callback is not a function');
    // on the way out, don't bother. it won't get fired anyway.
    if (process._exiting)
      return;

    var args;
    if (arguments.length > 1) {
      args = new Array(arguments.length - 1);
      for (var i = 1; i < arguments.length; i++)
        args[i - 1] = arguments[i];
    }

    nextTickQueue.push(
        new TickObject(callback, args, process.domain || null, false));
    tickInfo[kLength]++;
  }
}
