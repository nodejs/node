'use strict';

const {
  FunctionPrototypeBind,
} = primordials;

const {
  // For easy access to the nextTick state in the C++ land,
  // and to avoid unnecessary calls into JS land.
  tickInfo,
  // Used to run V8's micro task queue.
  runMicrotasks,
  setTickCallback,
  enqueueMicrotask,
} = internalBinding('task_queue');

const {
  setHasRejectionToWarn,
  hasRejectionToWarn,
  listenForRejections,
  processPromiseRejections,
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
  symbols: { async_id_symbol, trigger_async_id_symbol },
} = require('internal/async_hooks');
const FixedQueue = require('internal/fixed_queue');

const {
  validateFunction,
} = require('internal/validators');

const { AsyncResource } = require('async_hooks');

// *Must* match Environment::TickInfo::Fields in src/env.h.
const kHasTickScheduled = 0;

let queue = new FixedQueue();
let deferQueue = new FixedQueue();

// Should be in sync with RunNextTicksNative in node_task_queue.cc
function runNextTicks() {
  if (tickInfo[kHasTickScheduled] === 0 && !hasRejectionToWarn())
    runMicrotasks();
  if (tickInfo[kHasTickScheduled] === 0 && !hasRejectionToWarn())
    return;

  processTicksAndRejections();
}

function processTicksAndRejections() {
  let tock;
  do {
    while ((tock = queue.shift()) !== null) {
      const asyncId = tock[async_id_symbol];
      emitBefore(asyncId, tock[trigger_async_id_symbol], tock);

      try {
        const args = tock.args;
        const callback = tock.callback;
        if (args === undefined) {
          callback();
        } else {
          callback(...args);
        }
      } finally {
        if (destroyHooksExist())
          emitDestroy(asyncId);
      }

      emitAfter(asyncId);
    }
    runMicrotasks();

    const tmp = queue;
    queue = deferQueue;
    deferQueue = tmp;
  } while (!queue.isEmpty() || processPromiseRejections());
  tickInfo[kHasTickScheduled] = 0;
  setHasRejectionToWarn(false);
}

// `nextTick()` will not enqueue any callback when the process is about to
// exit since the callback would not have a chance to be executed.
function nextTick(callback, ...args) {
  validateFunction(callback, 'callback');

  if (process._exiting)
    return;

  if (tickInfo[kHasTickScheduled] === 0) {
    tickInfo[kHasTickScheduled] = 1;
  }

  const asyncId = newAsyncId();
  const triggerAsyncId = getDefaultTriggerAsyncId();
  const tickObject = {
    [async_id_symbol]: asyncId,
    [trigger_async_id_symbol]: triggerAsyncId,
    callback,
    args: args.length > 0 ? args : undefined,
  };
  if (initHooksExist())
    emitInit(asyncId, 'TickObject', triggerAsyncId, tickObject);
  queue.push(tickObject);
}

function deferTick(callback, ...args) {
  validateFunction(callback, 'callback');

  if (process._exiting)
    return;

  if (tickInfo[kHasTickScheduled] === 0) {
    tickInfo[kHasTickScheduled] = 1;
  }

  const asyncId = newAsyncId();
  const triggerAsyncId = getDefaultTriggerAsyncId();
  const tickObject = {
    [async_id_symbol]: asyncId,
    [trigger_async_id_symbol]: triggerAsyncId,
    callback,
    args: args.length > 0 ? args : undefined,
  };
  if (initHooksExist())
    emitInit(asyncId, 'TickObject', triggerAsyncId, tickObject);
  deferQueue.push(tickObject);
}

function runMicrotask() {
  this.runInAsyncScope(() => {
    const callback = this.callback;
    try {
      callback();
    } finally {
      this.emitDestroy();
    }
  });
}

const defaultMicrotaskResourceOpts = { requireManualDestroy: true };

function queueMicrotask(callback) {
  validateFunction(callback, 'callback');

  const asyncResource = new AsyncResource(
    'Microtask',
    defaultMicrotaskResourceOpts,
  );
  asyncResource.callback = callback;

  enqueueMicrotask(FunctionPrototypeBind(runMicrotask, asyncResource));
}

module.exports = {
  setupTaskQueue() {
    // Sets the per-isolate promise rejection callback
    listenForRejections();
    // Sets the callback to be run in every tick.
    setTickCallback(processTicksAndRejections);
    return {
      nextTick,
      deferTick,
      runNextTicks,
    };
  },
  queueMicrotask,
};
