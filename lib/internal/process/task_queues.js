'use strict';

const {
  Array,
  FunctionPrototypeBind,
  Symbol,
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

const AsyncContextFrame = require('internal/async_context_frame');

const async_context_frame = Symbol('kAsyncContextFrame');

// *Must* match Environment::TickInfo::Fields in src/env.h.
const kHasTickScheduled = 0;

function hasTickScheduled() {
  return tickInfo[kHasTickScheduled] === 1;
}

function setHasTickScheduled(value) {
  tickInfo[kHasTickScheduled] = value ? 1 : 0;
}

const queue = new FixedQueue();

// Object pools for performance optimization
const kTickObjectPoolSize = 512;
const kArgsPoolSize = 256;
const kMaxPooledArgsLength = 8;

// Tick object pool to reduce allocations
let tickObjectPool = [];
let tickObjectPoolSize = 0;

// Args pools for different lengths
let argsPools = new Array(kMaxPooledArgsLength + 1);
for (let i = 0; i <= kMaxPooledArgsLength; i++) {
  argsPools[i] = { pool: [], size: 0 };
}

function getPooledTickObject() {
  if (tickObjectPoolSize > 0) {
    return tickObjectPool[--tickObjectPoolSize];
  }
  return {};
}

function releaseTickObject(obj) {
  if (tickObjectPoolSize < kTickObjectPoolSize) {
    // Clear properties for reuse
    obj.callback = undefined;
    obj.args = undefined;
    tickObjectPool[tickObjectPoolSize++] = obj;
  }
}

function getPooledArgs(length) {
  if (length > kMaxPooledArgsLength) {
    return new Array(length);
  }
  
  const poolInfo = argsPools[length];
  if (poolInfo.size > 0) {
    return poolInfo.pool[--poolInfo.size];
  }
  return new Array(length);
}

function releaseArgs(arr) {
  const length = arr.length;
  if (length > kMaxPooledArgsLength) return;
  
  const poolInfo = argsPools[length];
  if (poolInfo.size < kArgsPoolSize) {
    // Clear array for reuse
    for (let i = 0; i < length; i++) {
      arr[i] = undefined;
    }
    poolInfo.pool[poolInfo.size++] = arr;
  }
}

// Batch processing for better performance
let processingBatch = false;

// Should be in sync with RunNextTicksNative in node_task_queue.cc
function runNextTicks() {
  if (!hasTickScheduled() && !hasRejectionToWarn())
    runMicrotasks();
  if (!hasTickScheduled() && !hasRejectionToWarn())
    return;

  processTicksAndRejections();
}

function processTicksAndRejections() {
  // Prevent recursive processing
  if (processingBatch) return;
  processingBatch = true;

  try {
    // Use batch processing for better cache locality and performance
    let processedThisRound;
    do {
      processedThisRound = queue.processBatch ? 
        queue.processBatch(processTickObject, 64) : 
        processTicksTraditional();
      
      // Run microtasks between batches to prevent starvation
      if (processedThisRound > 0) {
        runMicrotasks();
      }
    } while (processedThisRound > 0 || processPromiseRejections());
    
  } finally {
    processingBatch = false;
    setHasTickScheduled(false);
    setHasRejectionToWarn(false);
  }
}

// Optimized tick object processor for batch processing
function processTickObject(tock) {
  const priorContextFrame =
    AsyncContextFrame.exchange(tock[async_context_frame]);

  const asyncId = tock[async_id_symbol];
  emitBefore(asyncId, tock[trigger_async_id_symbol], tock);

  try {
    const callback = tock.callback;
    if (tock.args === undefined) {
      callback();
    } else {
      const args = tock.args;
      // Extended switch for better performance
      switch (args.length) {
        case 1: callback(args[0]); break;
        case 2: callback(args[0], args[1]); break;
        case 3: callback(args[0], args[1], args[2]); break;
        case 4: callback(args[0], args[1], args[2], args[3]); break;
        case 5: callback(args[0], args[1], args[2], args[3], args[4]); break;
        case 6: callback(args[0], args[1], args[2], args[3], args[4], args[5]); break;
        case 7: callback(args[0], args[1], args[2], args[3], args[4], args[5], args[6]); break;
        case 8: callback(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]); break;
        default: callback(...args); // Fallback for >8 args
      }
      // Return args to pool
      releaseArgs(args);
    }
  } finally {
    if (destroyHooksExist())
      emitDestroy(asyncId);
  }

  emitAfter(asyncId);
  AsyncContextFrame.set(priorContextFrame);
  
  // Return tick object to pool
  releaseTickObject(tock);
}

// Fallback to traditional processing if batch method not available
function processTicksTraditional() {
  let processed = 0;
  let tock;
  while ((tock = queue.shift()) !== null && processed < 64) {
    processTickObject(tock);
    processed++;
  }
  return processed;
}

// `nextTick()` will not enqueue any callback when the process is about to
// exit since the callback would not have a chance to be executed.
function nextTick(callback) {
  validateFunction(callback, 'callback');

  if (process._exiting)
    return;

  let args;
  const argsLength = arguments.length - 1;
  
  // Use pooled args array when possible
  if (argsLength > 0) {
    args = getPooledArgs(argsLength);
    // Optimized argument copying
    switch (argsLength) {
      case 1: args[0] = arguments[1]; break;
      case 2: args[0] = arguments[1]; args[1] = arguments[2]; break;
      case 3: args[0] = arguments[1]; args[1] = arguments[2]; args[2] = arguments[3]; break;
      case 4: 
        args[0] = arguments[1]; args[1] = arguments[2]; 
        args[2] = arguments[3]; args[3] = arguments[4]; 
        break;
      case 5:
        args[0] = arguments[1]; args[1] = arguments[2]; 
        args[2] = arguments[3]; args[3] = arguments[4];
        args[4] = arguments[5];
        break;
      case 6:
        args[0] = arguments[1]; args[1] = arguments[2]; 
        args[2] = arguments[3]; args[3] = arguments[4];
        args[4] = arguments[5]; args[5] = arguments[6];
        break;
      case 7:
        args[0] = arguments[1]; args[1] = arguments[2]; 
        args[2] = arguments[3]; args[3] = arguments[4];
        args[4] = arguments[5]; args[5] = arguments[6];
        args[6] = arguments[7];
        break;
      case 8:
        args[0] = arguments[1]; args[1] = arguments[2]; 
        args[2] = arguments[3]; args[3] = arguments[4];
        args[4] = arguments[5]; args[5] = arguments[6];
        args[6] = arguments[7]; args[7] = arguments[8];
        break;
      default:
        for (let i = 1; i < arguments.length; i++)
          args[i - 1] = arguments[i];
    }
  }

  if (queue.isEmpty())
    setHasTickScheduled(true);
    
  const asyncId = newAsyncId();
  const triggerAsyncId = getDefaultTriggerAsyncId();
  
  // Use pooled tick object
  const tickObject = getPooledTickObject();
  tickObject[async_id_symbol] = asyncId;
  tickObject[trigger_async_id_symbol] = triggerAsyncId;
  tickObject[async_context_frame] = AsyncContextFrame.current();
  tickObject.callback = callback;
  tickObject.args = args;

  if (initHooksExist())
    emitInit(asyncId, 'TickObject', triggerAsyncId, tickObject);
  queue.push(tickObject);
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
      runNextTicks,
    };
  },
  queueMicrotask,
};
