'use strict';

const {
  NumberIsSafeInteger,
  ReflectApply,
  Symbol,
} = primordials;

const {
  ERR_ASYNC_CALLBACK,
  ERR_INVALID_ASYNC_ID,
  ERR_INVALID_ARG_TYPE
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
  hasAsyncIdStack,
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
  initHooksExist,
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

class AsyncResource {
  constructor(type, opts = {}) {
    validateString(type, 'type');

    let triggerAsyncId = opts;
    let requireManualDestroy = false;
    if (typeof opts !== 'number') {
      triggerAsyncId = opts.triggerAsyncId === undefined ?
        getDefaultTriggerAsyncId() : opts.triggerAsyncId;
      requireManualDestroy = !!opts.requireManualDestroy;
    }

    // Unlike emitInitScript, AsyncResource doesn't supports null as the
    // triggerAsyncId.
    if (!NumberIsSafeInteger(triggerAsyncId) || triggerAsyncId < -1) {
      throw new ERR_INVALID_ASYNC_ID('triggerAsyncId', triggerAsyncId);
    }

    const asyncId = newAsyncId();
    this[async_id_symbol] = asyncId;
    this[trigger_async_id_symbol] = triggerAsyncId;

    if (initHooksExist()) {
      emitInit(asyncId, type, triggerAsyncId, this);
    }

    if (!requireManualDestroy) {
      // This prop name (destroyed) has to be synchronized with C++
      const destroyed = { destroyed: false };
      this[destroyedSymbol] = destroyed;
      registerDestroyHook(this, asyncId, destroyed);
    }
  }

  runInAsyncScope(fn, thisArg, ...args) {
    const asyncId = this[async_id_symbol];
    emitBefore(asyncId, this[trigger_async_id_symbol]);
    try {
      if (thisArg === undefined)
        return fn(...args);
      return ReflectApply(fn, thisArg, args);
    } finally {
      if (hasAsyncIdStack())
        emitAfter(asyncId);
    }
  }

  emitDestroy() {
    if (this[destroyedSymbol] !== undefined) {
      this[destroyedSymbol].destroyed = true;
    }
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


// AsyncLocal //

const kStack = Symbol('stack');
const kIsFirst = Symbol('is-first');
const kMap = Symbol('map');
const kOnChangedCb = Symbol('on-changed-cb');
const kHooks = Symbol('hooks');
const kSet = Symbol('set');

class AsyncLocal {
  constructor(options = {}) {
    if (typeof options !== 'object' || options === null)
      throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);

    const { onChangedCb = null } = options;
    if (onChangedCb !== null && typeof onChangedCb !== 'function')
      throw new ERR_INVALID_ARG_TYPE('options.onChangedCb',
                                     'function',
                                     onChangedCb);

    this[kOnChangedCb] = onChangedCb;
    this[kMap] = new Map();

    const fns = {
      init: (asyncId, type, triggerAsyncId, resource) => {
        // Propagate value from current id to new (execution graph)
        const value = this[kMap].get(executionAsyncId());
        if (value)
          this[kMap].set(asyncId, value);
      },

      destroy: (asyncId) => this[kSet](asyncId, null),
    };

    if (this[kOnChangedCb]) {
      // Change notification requires to keep a stack of async local values
      this[kStack] = [];
      // Indicates that first value was stored (before callback "missing")
      this[kIsFirst] = true;

      // Use before/after hooks to signal changes because of execution
      fns.before = (asyncId) => {
        const stack = this[kStack];
        const cVal = this[kMap].get(asyncId);
        const pVal = stack[stack.length - 1];
        stack.push(pVal);
        if (cVal !== pVal)
          this[kOnChangedCb](pVal, cVal, true);
      };

      fns.after = (asyncId) => {
        const stack = this[kStack];
        const pVal = this[kMap].get(asyncId);
        stack.pop();
        const cVal = stack[stack.length - 1];
        if (cVal !== pVal)
          this[kOnChangedCb](pVal, cVal, true);
      };
    }
    this[kHooks] = createHook(fns);
  }

  set value(val) {
    val = val === null ? undefined : val;
    const id = executionAsyncId();
    const onChangedCb = this[kOnChangedCb];
    let pVal;
    if (onChangedCb)
      pVal = this[kMap].get(id);

    this[kSet](id, val);

    if (onChangedCb && pVal !== val)
      onChangedCb(pVal, val, false);
  }

  get value() {
    return this[kMap].get(executionAsyncId());
  }

  [kSet](id, val) {
    const map = this[kMap];

    if (val == null) {
      map.delete(id);
      if (map.size === 0)
        this[kHooks].disable();

      if (this[kOnChangedCb]) {
        if (map.size === 0) {
          // Hooks have been disabled so next set is the first one
          this[kStack] = [];
          this[kIsFirst] = true;
        } else {
          const stack = this[kStack];
          stack[stack.length - 1] = undefined;
        }
      }
    } else {
      map.set(id, val);
      if (map.size === 1)
        this[kHooks].enable();

      if (this[kOnChangedCb]) {
        const stack = this[kStack];
        if (this[kIsFirst]) {
          // First value set => "simulate" before hook
          this[kIsFirst] = false;
          stack.push(val);
        } else {
          stack[stack.length - 1] = val;
        }
      }
    }
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
  // CLS API
  AsyncLocal
};
