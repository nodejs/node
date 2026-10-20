'use strict';

const {
  ArrayPrototypeUnshift,
  FunctionPrototypeBind,
  NumberIsSafeInteger,
  ObjectDefineProperties,
  ObjectFreeze,
  ReflectApply,
  Symbol,
} = primordials;

const {
  ERR_ASYNC_HOOKS_CREATE_HOOK_DISABLED,
  ERR_ASYNC_RESOURCE_DOMAIN_REMOVED,
  ERR_ASYNC_TYPE,
  ERR_INVALID_ASYNC_ID,
} = require('internal/errors').codes;
const {
  kEmptyObject,
} = require('internal/util');
const {
  validateFunction,
  validateString,
} = require('internal/validators');
const { getOptionValue } = require('internal/options');
const internal_async_hooks = require('internal/async_hooks');

const AsyncContextFrame = require('internal/async_context_frame');

// Get functions
// For userland AsyncResources, make sure to emit a destroy event when the
// resource gets gced.
const { registerDestroyHook } = internal_async_hooks;
const {
  asyncWrap,
  executionAsyncId,
  triggerAsyncId,
  // Private API
  hasAsyncIdStack,
  executionAsyncResource,
  // Internal Embedder API
  newAsyncId,
  getDefaultTriggerAsyncId,
  emitInit,
  emitBefore,
  emitAfter,
  emitDestroy,
  initHooksExist,
  destroyHooksExist,
  createHook: createHookInternal,
} = internal_async_hooks;

// Get symbols
const {
  async_id_symbol, trigger_async_id_symbol,
} = internal_async_hooks.symbols;

// Listener API //

function createHook(fns) {
  if (!getOptionValue('--allow-async-hooks')) {
    throw new ERR_ASYNC_HOOKS_CREATE_HOOK_DISABLED();
  }
  return createHookInternal(fns);
}


// Embedder API //

const destroyedSymbol = Symbol('destroyed');
const contextFrameSymbol = Symbol('context_frame');

class AsyncResource {
  constructor(type, opts = kEmptyObject) {
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

    this[contextFrameSymbol] = AsyncContextFrame.current();

    const asyncId = newAsyncId();
    this[async_id_symbol] = asyncId;
    this[trigger_async_id_symbol] = triggerAsyncId;

    if (initHooksExist()) {
      if (type.length === 0) {
        throw new ERR_ASYNC_TYPE(type);
      }

      emitInit(asyncId, type, triggerAsyncId, this);
    }

    if (!requireManualDestroy && destroyHooksExist()) {
      // This prop name (destroyed) has to be synchronized with C++
      const destroyed = { destroyed: false };
      this[destroyedSymbol] = destroyed;
      registerDestroyHook(this, asyncId, destroyed);
    }
  }

  runInAsyncScope(fn, thisArg, ...args) {
    const asyncId = this[async_id_symbol];
    emitBefore(asyncId, this[trigger_async_id_symbol], this);

    const contextFrame = this[contextFrameSymbol];
    const prior = AsyncContextFrame.exchange(contextFrame);
    try {
      return ReflectApply(fn, thisArg, args);
    } finally {
      AsyncContextFrame.set(prior);
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

  bind(fn, thisArg) {
    validateFunction(fn, 'fn');
    let bound;
    if (thisArg === undefined) {
      const resource = this;
      bound = function(...args) {
        ArrayPrototypeUnshift(args, fn, this);
        return ReflectApply(resource.runInAsyncScope, resource, args);
      };
    } else {
      bound = FunctionPrototypeBind(this.runInAsyncScope, this, fn, thisArg);
    }
    ObjectDefineProperties(bound, {
      'length': {
        __proto__: null,
        configurable: true,
        enumerable: false,
        value: fn.length,
        writable: false,
      },
    });
    return bound;
  }

  static bind(fn, type, thisArg) {
    type ||= fn.name;
    return (new AsyncResource(type || 'bound-anonymous-fn')).bind(fn, thisArg);
  }

  get domain() {
    throw new ERR_ASYNC_RESOURCE_DOMAIN_REMOVED();
  }
}

// Placing all exports down here because the exported classes won't export
// otherwise.
module.exports = {
  // Public API
  get AsyncLocalStorage() {
    return require('internal/async_local_storage/async_context_frame');
  },
  createHook,
  executionAsyncId,
  triggerAsyncId,
  executionAsyncResource,
  asyncWrapProviders: ObjectFreeze({ __proto__: null, ...asyncWrap.Providers }),
  // Embedder API
  AsyncResource,
};
