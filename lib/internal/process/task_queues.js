'use strict';

const {
  Array,
  FunctionPrototypeBind,
} = primordials;

const {
  // Used to run V8's micro task queue.
  runMicrotasks,
  setTickCallback,
  enqueueMicrotask
} = internalBinding('task_queue');

const {
  triggerUncaughtException
} = internalBinding('errors');

const {
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

const queue = new FixedQueue();

function runNextTicks() {
  let tock;
  do {
    while (tock = queue.shift()) {
      const asyncId = tock[async_id_symbol];
      emitBefore(asyncId, tock[trigger_async_id_symbol]);

      try {
        const callback = tock.callback;
        if (tock.args === undefined) {
          callback();
        } else {
          const args = tock.args;
          switch (args.length) {
            case 1: callback(args[0]); break;
            case 2: callback(args[0], args[1]); break;
            case 3: callback(args[0], args[1], args[2]); break;
            case 4: callback(args[0], args[1], args[2], args[3]); break;
            default: callback(...args);
          }
        }
      } finally {
        if (destroyHooksExist())
          emitDestroy(asyncId);
      }

      emitAfter(asyncId);
    }
    runMicrotasks();
  } while (!queue.isEmpty() || processPromiseRejections());
}

// `nextTick()` will not enqueue any callback when the process is about to
// exit since the callback would not have a chance to be executed.
function nextTick(callback) {
  if (typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  if (process._exiting)
    return;

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

  const asyncId = newAsyncId();
  const triggerAsyncId = getDefaultTriggerAsyncId();
  const tickObject = {
    [async_id_symbol]: asyncId,
    [trigger_async_id_symbol]: triggerAsyncId,
    callback,
    args
  };
  if (initHooksExist())
    emitInit(asyncId, 'TickObject', triggerAsyncId, tickObject);
  queue.push(tickObject);
}

let AsyncResource;
const defaultMicrotaskResourceOpts = { requireManualDestroy: true };
function createMicrotaskResource() {
  // Lazy load the async_hooks module
  if (AsyncResource === undefined) {
    AsyncResource = require('async_hooks').AsyncResource;
  }
  return new AsyncResource('Microtask', defaultMicrotaskResourceOpts);
}

function runMicrotask() {
  this.runInAsyncScope(() => {
    const callback = this.callback;
    try {
      callback();
    } catch (error) {
      // runInAsyncScope() swallows the error so we need to catch
      // it and handle it here.
      triggerUncaughtException(error, false /* fromPromise */);
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

  enqueueMicrotask(FunctionPrototypeBind(runMicrotask, asyncResource));
}

module.exports = {
  setupTaskQueue() {
    // Sets the per-isolate promise rejection callback
    listenForRejections();
    // Sets the callback to be run in every tick.
    setTickCallback(runNextTicks);
    return {
      nextTick,
      runNextTicks
    };
  },
  queueMicrotask
};
