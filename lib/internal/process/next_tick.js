'use strict';

const {
  // For easy access to the nextTick state in the C++ land,
  // and to avoid unnecessary calls into JS land.
  tickInfo,
  // Used to run V8's micro task queue.
  runMicrotasks,
  setTickCallback
} = internalBinding('task_queue');

const {
  setHasRejectionToWarn,
  hasRejectionToWarn,
  listenForRejections,
  processPromiseRejections
} = require('internal/process/promises');

const {
  getDefaultTriggerAsyncId,
  newAsyncId,
  initHooksExist,
  destroyHooksExist,
  emitInit,
  emitBefore,
  emitAfter,
  emitDestroy,
  symbols: { async_id_symbol, trigger_async_id_symbol }
} = require('internal/async_hooks');
const { ERR_INVALID_CALLBACK } = require('internal/errors').codes;
const FixedQueue = require('internal/fixed_queue');

// *Must* match Environment::TickInfo::Fields in src/env.h.
const kHasTickScheduled = 0;

function hasTickScheduled() {
  return tickInfo[kHasTickScheduled] === 1;
}
function setHasTickScheduled(value) {
  tickInfo[kHasTickScheduled] = value ? 1 : 0;
}

const queue = new FixedQueue();

function runNextTicks() {
  if (!hasTickScheduled() && !hasRejectionToWarn())
    runMicrotasks();
  if (!hasTickScheduled() && !hasRejectionToWarn())
    return;

  processTicksAndRejections();
}

function processTicksAndRejections() {
  let tock;
  do {
    while (tock = queue.shift()) {
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
    setHasTickScheduled(false);
    runMicrotasks();
  } while (!queue.isEmpty() || processPromiseRejections());
  setHasRejectionToWarn(false);
}

class TickObject {
  constructor(callback, args, triggerAsyncId) {
    // This must be set to null first to avoid function tracking
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
    throw new ERR_INVALID_CALLBACK();

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

  if (queue.isEmpty())
    setHasTickScheduled(true);
  queue.push(new TickObject(callback, args, getDefaultTriggerAsyncId()));
}

// TODO(joyeecheung): make this a factory class so that node.js can
// control the side effects caused by the initializers.
exports.setup = function() {
  // Sets the per-isolate promise rejection callback
  listenForRejections();
  // Sets the callback to be run in every tick.
  setTickCallback(processTicksAndRejections);
  return {
    nextTick,
    runNextTicks
  };
};
