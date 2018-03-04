'use strict';

exports.setup = setupNextTick;
// Will be overwritten when setupNextTick() is called.
exports.nextTick = null;

function setupNextTick() {
  const {
    getDefaultTriggerAsyncId,
    newAsyncId,
    initHooksExist,
    destroyHooksExist,
    emitInit,
    emitBefore,
    emitAfter,
    emitDestroy,
    symbols: { async_id_symbol, trigger_async_id_symbol },
  } = require('internal/async_hooks');
  const promises = require('internal/process/promises');
  const errors = require('internal/errors');
  const { emitPromiseRejectionWarnings } = promises;

  // tickInfo is used so that the C++ code in src/node.cc can
  // have easy access to our nextTick state, and avoid unnecessary
  // calls into JS land.
  // runMicrotasks is used to run V8's micro task queue.
  const [
    tickInfo,
    runMicrotasks,
  ] = process._setupNextTick(_tickCallback);

  // *Must* match Environment::TickInfo::Fields in src/env.h.
  const kHasScheduled = 0;
  const kHasPromiseRejections = 1;

  // Queue size for each tick array. Must be a factor of two.
  const kQueueSize = 2048;
  const kQueueMask = kQueueSize - 1;

  class FixedQueue {
    constructor() {
      this.bottom = 0;
      this.top = 0;
      this.list = new Array(kQueueSize);
      this.next = null;
    }

    push(data) {
      this.list[this.top] = data;
      this.top = (this.top + 1) & kQueueMask;
    }

    shift() {
      const next = this.list[this.bottom];
      if (next === undefined) return null;
      this.list[this.bottom] = undefined;
      this.bottom = (this.bottom + 1) & kQueueMask;
      return next;
    }
  }

  var head = new FixedQueue();
  var tail = head;

  function push(data) {
    if (head.bottom === head.top) {
      if (head.list[head.top] !== undefined)
        head = head.next = new FixedQueue();
      else
        tickInfo[kHasScheduled] = 1;
    }
    head.push(data);
  }

  function shift() {
    const next = tail.shift();
    if (tail.top === tail.bottom) {
      if (tail.next)
        tail = tail.next;
      else
        tickInfo[kHasScheduled] = 0;
    }
    return next;
  }

  process.nextTick = nextTick;
  // Needs to be accessible from beyond this scope.
  process._tickCallback = _tickCallback;

  // Set the nextTick() function for internal usage.
  exports.nextTick = internalNextTick;

  function _tickCallback() {
    let tock;
    do {
      while (tock = shift()) {
        const asyncId = tock[async_id_symbol];
        emitBefore(asyncId, tock[trigger_async_id_symbol]);
        // emitDestroy() places the async_id_symbol into an asynchronous queue
        // that calls the destroy callback in the future. It's called before
        // calling tock.callback so destroy will be called even if the callback
        // throws an exception that is handled by 'uncaughtException' or a
        // domain.
        // TODO(trevnorris): This is a bit of a hack. It relies on the fact
        // that nextTick() doesn't allow the event loop to proceed, but if
        // any async hooks are enabled during the callback's execution then
        // this tock's after hook will be called, but not its destroy hook.
        if (destroyHooksExist())
          emitDestroy(asyncId);

        const callback = tock.callback;
        if (tock.args === undefined)
          callback();
        else
          Reflect.apply(callback, undefined, tock.args);

        emitAfter(asyncId);
      }
      runMicrotasks();
    } while (head.top !== head.bottom || emitPromiseRejectionWarnings());
    tickInfo[kHasPromiseRejections] = 0;
  }

  class TickObject {
    constructor(callback, args, triggerAsyncId) {
      // this must be set to null first to avoid function tracking
      // on the hidden class, revisit in V8 versions after 6.2
      this.callback = null;
      this.callback = callback;
      this.args = args;

      const asyncId = newAsyncId();
      this[async_id_symbol] = asyncId;
      this[trigger_async_id_symbol] = triggerAsyncId;

      if (initHooksExist()) {
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

    push(new TickObject(callback, args, getDefaultTriggerAsyncId()));
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
    push(new TickObject(callback, args, triggerAsyncId));
  }
}
