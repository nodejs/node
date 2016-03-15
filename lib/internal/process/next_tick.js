'use strict';

exports.setup = setupNextTick;

function setupNextTick() {
  const promises = require('internal/process/promises');
  const emitPendingUnhandledRejections = promises.setup(scheduleMicrotasks);
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

    nextTickQueue.push({
      callback: runMicrotasksCallback,
      domain: null
    });

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

  // Run callbacks that have no domain.
  // Using domains will cause this to be overridden.
  function _tickCallback() {
    var callback, args, tock;

    do {
      while (tickInfo[kIndex] < tickInfo[kLength]) {
        tock = nextTickQueue[tickInfo[kIndex]++];
        callback = tock.callback;
        args = tock.args;
        // Using separate callback execution functions allows direct
        // callback invocation with small numbers of arguments to avoid the
        // performance hit associated with using `fn.apply()`
        _combinedTickCallback(args, callback);
        if (1e4 < tickInfo[kIndex])
          tickDone();
      }
      tickDone();
      _runMicrotasks();
      emitPendingUnhandledRejections();
    } while (tickInfo[kLength] !== 0);
  }

  function _tickDomainCallback() {
    var callback, domain, args, tock;

    do {
      while (tickInfo[kIndex] < tickInfo[kLength]) {
        tock = nextTickQueue[tickInfo[kIndex]++];
        callback = tock.callback;
        domain = tock.domain;
        args = tock.args;
        if (domain)
          domain.enter();
        // Using separate callback execution functions allows direct
        // callback invocation with small numbers of arguments to avoid the
        // performance hit associated with using `fn.apply()`
        _combinedTickCallback(args, callback);
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

  function TickObject(c, args) {
    this.callback = c;
    this.domain = process.domain || null;
    this.args = args;
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

    nextTickQueue.push(new TickObject(callback, args));
    tickInfo[kLength]++;
  }
}
