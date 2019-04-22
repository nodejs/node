'use strict';

const { FunctionPrototype } = primordials;

const {
  // For easy access to the nextTick state in the C++ land,
  // and to avoid unnecessary calls into JS land.
  tickInfo,
  // Used to run V8's micro task queue.
  runMicrotasks,
  setTickCallback,
  enqueueMicrotask,
  triggerFatalException
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
const {
  ERR_INVALID_CALLBACK,
  ERR_INVALID_ARG_TYPE
} = require('internal/errors').codes;
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
      const {
        [async_id_symbol]: asyncId,
        [trigger_async_id_symbol]: triggerAsyncId,
        callback,
        args
      } = tock;

      emitBefore(asyncId, triggerAsyncId);

      try {
        if (args === undefined)
          callback();
        else
          callback(...args);
      } finally {
        if (destroyHooksExist())
          emitDestroy(asyncId);
      }

      emitAfter(asyncId);
    }
    setHasTickScheduled(false);
    runMicrotasks();
  } while (!queue.isEmpty() || processPromiseRejections());
  setHasRejectionToWarn(false);
}

class TickObject {
  constructor(callback, args, asyncId, triggerAsyncId) {
    // This must be set to null first to avoid function tracking
    // on the hidden class, revisit in V8 versions after 7.4
    this.callback = null;
    this.callback = callback;
    this.args = args;

    this[async_id_symbol] = asyncId;
    this[trigger_async_id_symbol] = triggerAsyncId;

    if (initHooksExist())
      emitInit(asyncId, 'TickObject', triggerAsyncId, this);
  }
}

// `nextTick()` will not enqueue any callback when the process is about to
// exit since the callback would not have a chance to be executed.
function nextTick(callback) {
  if (typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  let args;
  switch (arguments.length) {
    case 1: break;
    case 2: args = [arguments[1]]; break;
    case 3: args = [arguments[1], arguments[2]]; break;
    case 4: args = [arguments[1], arguments[2], arguments[3]]; break;
    default:
      args = new Array(arguments.length - 1);
      for (let i = 1; i < arguments.length; i++)
        args[i - 1] = arguments[i];
  }

  if (queue.isEmpty() && !process._exiting)
    setHasTickScheduled(true);

  queue.push(new TickObject(callback,
                            args,
                            newAsyncId(),
                            getDefaultTriggerAsyncId()));
}

let AsyncResource;
function createMicrotaskResource() {
  // Lazy load the async_hooks module
  if (!AsyncResource) {
    AsyncResource = require('async_hooks').AsyncResource;
  }
  return new AsyncResource('Microtask', {
    triggerAsyncId: getDefaultTriggerAsyncId(),
    requireManualDestroy: true,
  });
}

function runMicrotask() {
  this.runInAsyncScope(() => {
    const callback = this.callback;
    try {
      callback();
    } catch (error) {
      // TODO(devsnek): Remove this if
      // https://bugs.chromium.org/p/v8/issues/detail?id=8326
      // is resolved such that V8 triggers the fatal exception
      // handler for microtasks.
      triggerFatalException(error, false /* fromPromise */);
    } finally {
      this.emitDestroy();
    }
  });
}

function queueMicrotask(callback) {
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_ARG_TYPE('callback', 'function', callback);
  }

  const asyncResource = createMicrotaskResource();
  asyncResource.callback = callback;

  enqueueMicrotask(FunctionPrototype.bind(runMicrotask, asyncResource));
}

module.exports = {
  setupTaskQueue() {
    // Sets the per-isolate promise rejection callback
    listenForRejections();
    // Sets the callback to be run in every tick.
    setTickCallback(processTicksAndRejections);
    return {
      nextTick,
      runNextTicks
    };
  },
  queueMicrotask
};
