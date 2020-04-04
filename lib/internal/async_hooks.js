'use strict';

const {
  Error,
  FunctionPrototypeBind,
  ObjectDefineProperty,
  Symbol,
} = primordials;

const async_wrap = internalBinding('async_wrap');
/* async_hook_fields is a Uint32Array wrapping the uint32_t array of
 * Environment::AsyncHooks::fields_[]. Each index tracks the number of active
 * hooks for each type.
 *
 * async_id_fields is a Float64Array wrapping the double array of
 * Environment::AsyncHooks::async_id_fields_[]. Each index contains the ids for
 * the various asynchronous states of the application. These are:
 *  kExecutionAsyncId: The async_id assigned to the resource responsible for the
 *    current execution stack.
 *  kTriggerAsyncId: The async_id of the resource that caused (or 'triggered')
 *    the resource corresponding to the current execution stack.
 *  kAsyncIdCounter: Incremental counter tracking the next assigned async_id.
 *  kDefaultTriggerAsyncId: Written immediately before a resource's constructor
 *    that sets the value of the init()'s triggerAsyncId. The precedence order
 *    of retrieving the triggerAsyncId value is:
 *    1. the value passed directly to the constructor
 *    2. value set in kDefaultTriggerAsyncId
 *    3. executionAsyncId of the current resource.
 *
 * async_ids_stack is a Float64Array that contains part of the async ID
 * stack. Each pushAsyncContext() call adds two doubles to it, and each
 * popAsyncContext() call removes two doubles from it.
 * It has a fixed size, so if that is exceeded, calls to the native
 * side are used instead in pushAsyncContext() and popAsyncContext().
 */
const {
  async_hook_fields,
  async_id_fields,
  execution_async_resources,
  owner_symbol
} = async_wrap;
// Store the pair executionAsyncId and triggerAsyncId in a std::stack on
// Environment::AsyncHooks::async_ids_stack_ tracks the resource responsible for
// the current execution stack. This is unwound as each resource exits. In the
// case of a fatal exception this stack is emptied after calling each hook's
// after() callback.
const {
  pushAsyncContext: pushAsyncContext_,
  popAsyncContext: popAsyncContext_
} = async_wrap;
// For performance reasons, only track Promises when a hook is enabled.
const { enablePromiseHook, disablePromiseHook } = async_wrap;
// Properties in active_hooks are used to keep track of the set of hooks being
// executed in case another hook is enabled/disabled. The new set of hooks is
// then restored once the active set of hooks is finished executing.
const active_hooks = {
  // Array of all AsyncHooks that will be iterated whenever an async event
  // fires. Using var instead of (preferably const) in order to assign
  // active_hooks.tmp_array if a hook is enabled/disabled during hook
  // execution.
  array: [],
  // Use a counter to track nested calls of async hook callbacks and make sure
  // the active_hooks.array isn't altered mid execution.
  call_depth: 0,
  // Use to temporarily store and updated active_hooks.array if the user
  // enables or disables a hook while hooks are being processed. If a hook is
  // enabled() or disabled() during hook execution then the current set of
  // active hooks is duplicated and set equal to active_hooks.tmp_array. Any
  // subsequent changes are on the duplicated array. When all hooks have
  // completed executing active_hooks.tmp_array is assigned to
  // active_hooks.array.
  tmp_array: null,
  // Keep track of the field counts held in active_hooks.tmp_array. Because the
  // async_hook_fields can't be reassigned, store each uint32 in an array that
  // is written back to async_hook_fields when active_hooks.array is restored.
  tmp_fields: null
};

const { registerDestroyHook } = async_wrap;
const { enqueueMicrotask } = internalBinding('task_queue');

// Each constant tracks how many callbacks there are for any given step of
// async execution. These are tracked so if the user didn't include callbacks
// for a given step, that step can bail out early.
const { kInit, kBefore, kAfter, kDestroy, kTotals, kPromiseResolve,
        kCheck, kExecutionAsyncId, kAsyncIdCounter, kTriggerAsyncId,
        kDefaultTriggerAsyncId, kStackLength } = async_wrap.constants;

// Used in AsyncHook and AsyncResource.
const async_id_symbol = Symbol('asyncId');
const trigger_async_id_symbol = Symbol('triggerAsyncId');
const init_symbol = Symbol('init');
const before_symbol = Symbol('before');
const after_symbol = Symbol('after');
const destroy_symbol = Symbol('destroy');
const promise_resolve_symbol = Symbol('promiseResolve');
const emitBeforeNative = emitHookFactory(before_symbol, 'emitBeforeNative');
const emitAfterNative = emitHookFactory(after_symbol, 'emitAfterNative');
const emitDestroyNative = emitHookFactory(destroy_symbol, 'emitDestroyNative');
const emitPromiseResolveNative =
    emitHookFactory(promise_resolve_symbol, 'emitPromiseResolveNative');

const topLevelResource = {};

function executionAsyncResource() {
  const index = async_hook_fields[kStackLength] - 1;
  if (index === -1) return topLevelResource;
  const resource = execution_async_resources[index];
  return resource;
}

// Used to fatally abort the process if a callback throws.
function fatalError(e) {
  if (typeof e.stack === 'string') {
    process._rawDebug(e.stack);
  } else {
    const o = { message: e };
    // eslint-disable-next-line no-restricted-syntax
    Error.captureStackTrace(o, fatalError);
    process._rawDebug(o.stack);
  }

  const { getOptionValue } = require('internal/options');
  if (getOptionValue('--abort-on-uncaught-exception')) {
    process.abort();
  }
  process.exit(1);
}


// Emit From Native //

// Used by C++ to call all init() callbacks. Because some state can be setup
// from C++ there's no need to perform all the same operations as in
// emitInitScript.
function emitInitNative(asyncId, type, triggerAsyncId, resource) {
  active_hooks.call_depth += 1;
  // Use a single try/catch for all hooks to avoid setting up one per iteration.
  try {
    // Using var here instead of let because "for (var ...)" is faster than let.
    // Refs: https://github.com/nodejs/node/pull/30380#issuecomment-552948364
    for (var i = 0; i < active_hooks.array.length; i++) {
      if (typeof active_hooks.array[i][init_symbol] === 'function') {
        active_hooks.array[i][init_symbol](
          asyncId, type, triggerAsyncId,
          resource
        );
      }
    }
  } catch (e) {
    fatalError(e);
  } finally {
    active_hooks.call_depth -= 1;
  }

  // Hooks can only be restored if there have been no recursive hook calls.
  // Also the active hooks do not need to be restored if enable()/disable()
  // weren't called during hook execution, in which case active_hooks.tmp_array
  // will be null.
  if (active_hooks.call_depth === 0 && active_hooks.tmp_array !== null) {
    restoreActiveHooks();
  }
}

// Called from native. The asyncId stack handling is taken care of there
// before this is called.
function emitHook(symbol, asyncId) {
  active_hooks.call_depth += 1;
  // Use a single try/catch for all hook to avoid setting up one per
  // iteration.
  try {
    // Using var here instead of let because "for (var ...)" is faster than let.
    // Refs: https://github.com/nodejs/node/pull/30380#issuecomment-552948364
    for (var i = 0; i < active_hooks.array.length; i++) {
      if (typeof active_hooks.array[i][symbol] === 'function') {
        active_hooks.array[i][symbol](asyncId);
      }
    }
  } catch (e) {
    fatalError(e);
  } finally {
    active_hooks.call_depth -= 1;
  }

  // Hooks can only be restored if there have been no recursive hook calls.
  // Also the active hooks do not need to be restored if enable()/disable()
  // weren't called during hook execution, in which case
  // active_hooks.tmp_array will be null.
  if (active_hooks.call_depth === 0 && active_hooks.tmp_array !== null) {
    restoreActiveHooks();
  }
}

function emitHookFactory(symbol, name) {
  const fn = FunctionPrototypeBind(emitHook, undefined, symbol);

  // Set the name property of the function as it looks good in the stack trace.
  ObjectDefineProperty(fn, 'name', {
    value: name
  });
  return fn;
}

// Manage Active Hooks //

function getHookArrays() {
  if (active_hooks.call_depth === 0)
    return [active_hooks.array, async_hook_fields];
  // If this hook is being enabled while in the middle of processing the array
  // of currently active hooks then duplicate the current set of active hooks
  // and store this there. This shouldn't fire until the next time hooks are
  // processed.
  if (active_hooks.tmp_array === null)
    storeActiveHooks();
  return [active_hooks.tmp_array, active_hooks.tmp_fields];
}


function storeActiveHooks() {
  active_hooks.tmp_array = active_hooks.array.slice();
  // Don't want to make the assumption that kInit to kDestroy are indexes 0 to
  // 4. So do this the long way.
  active_hooks.tmp_fields = [];
  copyHooks(active_hooks.tmp_fields, async_hook_fields);
}

function copyHooks(destination, source) {
  destination[kInit] = source[kInit];
  destination[kBefore] = source[kBefore];
  destination[kAfter] = source[kAfter];
  destination[kDestroy] = source[kDestroy];
  destination[kPromiseResolve] = source[kPromiseResolve];
}


// Then restore the correct hooks array in case any hooks were added/removed
// during hook callback execution.
function restoreActiveHooks() {
  active_hooks.array = active_hooks.tmp_array;
  copyHooks(async_hook_fields, active_hooks.tmp_fields);

  active_hooks.tmp_array = null;
  active_hooks.tmp_fields = null;
}


let wantPromiseHook = false;
function enableHooks() {
  async_hook_fields[kCheck] += 1;

  wantPromiseHook = true;
  enablePromiseHook();
}

function disableHooks() {
  async_hook_fields[kCheck] -= 1;

  wantPromiseHook = false;
  // Delay the call to `disablePromiseHook()` because we might currently be
  // between the `before` and `after` calls of a Promise.
  enqueueMicrotask(disablePromiseHookIfNecessary);
}

function disablePromiseHookIfNecessary() {
  if (!wantPromiseHook)
    disablePromiseHook();
}

// Internal Embedder API //

// Increment the internal id counter and return the value. Important that the
// counter increment first. Since it's done the same way in
// Environment::new_async_uid()
function newAsyncId() {
  return ++async_id_fields[kAsyncIdCounter];
}

function getOrSetAsyncId(object) {
  if (object.hasOwnProperty(async_id_symbol)) {
    return object[async_id_symbol];
  }

  return object[async_id_symbol] = newAsyncId();
}


// Return the triggerAsyncId meant for the constructor calling it. It's up to
// the user to safeguard this call and make sure it's zero'd out when the
// constructor is complete.
function getDefaultTriggerAsyncId() {
  const defaultTriggerAsyncId = async_id_fields[kDefaultTriggerAsyncId];
  // If defaultTriggerAsyncId isn't set, use the executionAsyncId
  if (defaultTriggerAsyncId < 0)
    return async_id_fields[kExecutionAsyncId];
  return defaultTriggerAsyncId;
}


function clearDefaultTriggerAsyncId() {
  async_id_fields[kDefaultTriggerAsyncId] = -1;
}


function defaultTriggerAsyncIdScope(triggerAsyncId, block, ...args) {
  if (triggerAsyncId === undefined)
    return block(...args);
  // CHECK(NumberIsSafeInteger(triggerAsyncId))
  // CHECK(triggerAsyncId > 0)
  const oldDefaultTriggerAsyncId = async_id_fields[kDefaultTriggerAsyncId];
  async_id_fields[kDefaultTriggerAsyncId] = triggerAsyncId;

  try {
    return block(...args);
  } finally {
    async_id_fields[kDefaultTriggerAsyncId] = oldDefaultTriggerAsyncId;
  }
}

function hasHooks(key) {
  return async_hook_fields[key] > 0;
}

function enabledHooksExist() {
  return hasHooks(kCheck);
}

function initHooksExist() {
  return hasHooks(kInit);
}

function afterHooksExist() {
  return hasHooks(kAfter);
}

function destroyHooksExist() {
  return hasHooks(kDestroy);
}


function emitInitScript(asyncId, type, triggerAsyncId, resource) {
  // Short circuit all checks for the common case. Which is that no hooks have
  // been set. Do this to remove performance impact for embedders (and core).
  if (!hasHooks(kInit))
    return;

  if (triggerAsyncId === null) {
    triggerAsyncId = getDefaultTriggerAsyncId();
  }

  emitInitNative(asyncId, type, triggerAsyncId, resource);
}


function emitBeforeScript(asyncId, triggerAsyncId, resource) {
  pushAsyncContext(asyncId, triggerAsyncId, resource);

  if (hasHooks(kBefore))
    emitBeforeNative(asyncId);
}


function emitAfterScript(asyncId) {
  if (hasHooks(kAfter))
    emitAfterNative(asyncId);

  popAsyncContext(asyncId);
}


function emitDestroyScript(asyncId) {
  // Return early if there are no destroy callbacks, or invalid asyncId.
  if (!hasHooks(kDestroy) || asyncId <= 0)
    return;
  async_wrap.queueDestroyAsyncId(asyncId);
}


// Keep in sync with Environment::AsyncHooks::clear_async_id_stack
// in src/env-inl.h.
function clearAsyncIdStack() {
  async_id_fields[kExecutionAsyncId] = 0;
  async_id_fields[kTriggerAsyncId] = 0;
  async_hook_fields[kStackLength] = 0;
  execution_async_resources.splice(0, execution_async_resources.length);
}


function hasAsyncIdStack() {
  return hasHooks(kStackLength);
}


// This is the equivalent of the native push_async_ids() call.
function pushAsyncContext(asyncId, triggerAsyncId, resource) {
  const offset = async_hook_fields[kStackLength];
  if (offset * 2 >= async_wrap.async_ids_stack.length)
    return pushAsyncContext_(asyncId, triggerAsyncId, resource);
  async_wrap.async_ids_stack[offset * 2] = async_id_fields[kExecutionAsyncId];
  async_wrap.async_ids_stack[offset * 2 + 1] = async_id_fields[kTriggerAsyncId];
  execution_async_resources[offset] = resource;
  async_hook_fields[kStackLength]++;
  async_id_fields[kExecutionAsyncId] = asyncId;
  async_id_fields[kTriggerAsyncId] = triggerAsyncId;
}


// This is the equivalent of the native pop_async_ids() call.
function popAsyncContext(asyncId) {
  const stackLength = async_hook_fields[kStackLength];
  if (stackLength === 0) return false;

  if (enabledHooksExist() && async_id_fields[kExecutionAsyncId] !== asyncId) {
    // Do the same thing as the native code (i.e. crash hard).
    return popAsyncContext_(asyncId);
  }

  const offset = stackLength - 1;
  async_id_fields[kExecutionAsyncId] = async_wrap.async_ids_stack[2 * offset];
  async_id_fields[kTriggerAsyncId] = async_wrap.async_ids_stack[2 * offset + 1];
  execution_async_resources.pop();
  async_hook_fields[kStackLength] = offset;
  return offset > 0;
}


function executionAsyncId() {
  return async_id_fields[kExecutionAsyncId];
}

function triggerAsyncId() {
  return async_id_fields[kTriggerAsyncId];
}


module.exports = {
  executionAsyncId,
  triggerAsyncId,
  // Private API
  getHookArrays,
  symbols: {
    async_id_symbol, trigger_async_id_symbol,
    init_symbol, before_symbol, after_symbol, destroy_symbol,
    promise_resolve_symbol, owner_symbol
  },
  constants: {
    kInit, kBefore, kAfter, kDestroy, kTotals, kPromiseResolve
  },
  enableHooks,
  disableHooks,
  clearDefaultTriggerAsyncId,
  clearAsyncIdStack,
  hasAsyncIdStack,
  executionAsyncResource,
  // Internal Embedder API
  newAsyncId,
  getOrSetAsyncId,
  getDefaultTriggerAsyncId,
  defaultTriggerAsyncIdScope,
  enabledHooksExist,
  initHooksExist,
  afterHooksExist,
  destroyHooksExist,
  emitInit: emitInitScript,
  emitBefore: emitBeforeScript,
  emitAfter: emitAfterScript,
  emitDestroy: emitDestroyScript,
  registerDestroyHook,
  nativeHooks: {
    init: emitInitNative,
    before: emitBeforeNative,
    after: emitAfterNative,
    destroy: emitDestroyNative,
    promise_resolve: emitPromiseResolveNative
  }
};
