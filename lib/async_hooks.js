'use strict';

const {
  ERR_ASYNC_CALLBACK,
  ERR_INVALID_ASYNC_ID
} = require('internal/errors').codes;
const { validateString } = require('internal/validators');
const internal_async_hooks = require('internal/async_hooks');

// Get functions
// For userland AsyncResources, make sure to emit a destroy event when the
// resource gets gced.
const { registerDestroyHook } = internal_async_hooks;
const {
  executionAsyncId,
  triggerAsyncId,
  // Private API
  getHookArrays,
  enableHooks,
  disableHooks,
  // Internal Embedder API
  newAsyncId,
  getDefaultTriggerAsyncId,
  emitInit,
  emitBefore,
  emitAfter,
  emitDestroy,
} = internal_async_hooks;

// Get symbols
const {
  async_id_symbol, trigger_async_id_symbol,
  init_symbol, before_symbol, after_symbol, destroy_symbol,
  promise_resolve_symbol
} = internal_async_hooks.symbols;

// Get constants
const {
  kInit, kBefore, kAfter, kDestroy, kTotals, kPromiseResolve,
} = internal_async_hooks.constants;

// Listener API //

class AsyncHook {
  constructor({ init, before, after, destroy, promiseResolve }) {
    if (init !== undefined && typeof init !== 'function')
      throw new ERR_ASYNC_CALLBACK('hook.init');
    if (before !== undefined && typeof before !== 'function')
      throw new ERR_ASYNC_CALLBACK('hook.before');
    if (after !== undefined && typeof after !== 'function')
      throw new ERR_ASYNC_CALLBACK('hook.after');
    if (destroy !== undefined && typeof destroy !== 'function')
      throw new ERR_ASYNC_CALLBACK('hook.destroy');
    if (promiseResolve !== undefined && typeof promiseResolve !== 'function')
      throw new ERR_ASYNC_CALLBACK('hook.promiseResolve');

    this[init_symbol] = init;
    this[before_symbol] = before;
    this[after_symbol] = after;
    this[destroy_symbol] = destroy;
    this[promise_resolve_symbol] = promiseResolve;
  }

  enable() {
    // The set of callbacks for a hook should be the same regardless of whether
    // enable()/disable() are run during their execution. The following
    // references are reassigned to the tmp arrays if a hook is currently being
    // processed.
    const [hooks_array, hook_fields] = getHookArrays();

    // Each hook is only allowed to be added once.
    if (hooks_array.includes(this))
      return this;

    const prev_kTotals = hook_fields[kTotals];

    // createHook() has already enforced that the callbacks are all functions,
    // so here simply increment the count of whether each callbacks exists or
    // not.
    hook_fields[kTotals] = hook_fields[kInit] += +!!this[init_symbol];
    hook_fields[kTotals] += hook_fields[kBefore] += +!!this[before_symbol];
    hook_fields[kTotals] += hook_fields[kAfter] += +!!this[after_symbol];
    hook_fields[kTotals] += hook_fields[kDestroy] += +!!this[destroy_symbol];
    hook_fields[kTotals] +=
        hook_fields[kPromiseResolve] += +!!this[promise_resolve_symbol];
    hooks_array.push(this);

    if (prev_kTotals === 0 && hook_fields[kTotals] > 0) {
      enableHooks();
    }

    return this;
  }

  disable() {
    const [hooks_array, hook_fields] = getHookArrays();

    const index = hooks_array.indexOf(this);
    if (index === -1)
      return this;

    const prev_kTotals = hook_fields[kTotals];

    hook_fields[kTotals] = hook_fields[kInit] -= +!!this[init_symbol];
    hook_fields[kTotals] += hook_fields[kBefore] -= +!!this[before_symbol];
    hook_fields[kTotals] += hook_fields[kAfter] -= +!!this[after_symbol];
    hook_fields[kTotals] += hook_fields[kDestroy] -= +!!this[destroy_symbol];
    hook_fields[kTotals] +=
        hook_fields[kPromiseResolve] -= +!!this[promise_resolve_symbol];
    hooks_array.splice(index, 1);

    if (prev_kTotals > 0 && hook_fields[kTotals] === 0) {
      disableHooks();
    }

    return this;
  }
}


function createHook(fns) {
  return new AsyncHook(fns);
}


// Embedder API //

const destroyedSymbol = Symbol('destroyed');

let emitBeforeAfterWarning = true;
function showEmitBeforeAfterWarning() {
  if (emitBeforeAfterWarning) {
    process.emitWarning(
      'asyncResource.emitBefore and emitAfter are deprecated. Please use ' +
      'asyncResource.runInAsyncScope instead',
      'DeprecationWarning', 'DEP0098');
    emitBeforeAfterWarning = false;
  }
}

class AsyncResource {
  constructor(type, opts = {}) {
    validateString(type, 'type');

    if (typeof opts === 'number') {
      opts = { triggerAsyncId: opts, requireManualDestroy: false };
    } else if (opts.triggerAsyncId === undefined) {
      opts.triggerAsyncId = getDefaultTriggerAsyncId();
    }

    // Unlike emitInitScript, AsyncResource doesn't supports null as the
    // triggerAsyncId.
    const triggerAsyncId = opts.triggerAsyncId;
    if (!Number.isSafeInteger(triggerAsyncId) || triggerAsyncId < -1) {
      throw new ERR_INVALID_ASYNC_ID('triggerAsyncId', triggerAsyncId);
    }

    this[async_id_symbol] = newAsyncId();
    this[trigger_async_id_symbol] = triggerAsyncId;
    // This prop name (destroyed) has to be synchronized with C++
    this[destroyedSymbol] = { destroyed: false };

    emitInit(
      this[async_id_symbol], type, this[trigger_async_id_symbol], this
    );

    if (!opts.requireManualDestroy) {
      registerDestroyHook(this, this[async_id_symbol], this[destroyedSymbol]);
    }
  }

  emitBefore() {
    showEmitBeforeAfterWarning();
    emitBefore(this[async_id_symbol], this[trigger_async_id_symbol]);
    return this;
  }

  emitAfter() {
    showEmitBeforeAfterWarning();
    emitAfter(this[async_id_symbol]);
    return this;
  }

  runInAsyncScope(fn, thisArg, ...args) {
    emitBefore(this[async_id_symbol], this[trigger_async_id_symbol]);
    let ret;
    try {
      ret = Reflect.apply(fn, thisArg, args);
    } finally {
      emitAfter(this[async_id_symbol]);
    }
    return ret;
  }

  emitDestroy() {
    this[destroyedSymbol].destroyed = true;
    emitDestroy(this[async_id_symbol]);
    return this;
  }

  asyncId() {
    return this[async_id_symbol];
  }

  triggerAsyncId() {
    return this[trigger_async_id_symbol];
  }
}


// Placing all exports down here because the exported classes won't export
// otherwise.
module.exports = {
  // Public API
  createHook,
  executionAsyncId,
  triggerAsyncId,
  // Embedder API
  AsyncResource,
};
