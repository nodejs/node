'use strict';

const async_wrap = process.binding('async_wrap');
/* Both these arrays are used to communicate between JS and C++ with as little
 * overhead as possible.
 *
 * async_hook_fields is a Uint32Array() that communicates the number of each
 * type of active hooks of each type and wraps the uin32_t array of
 * node::Environment::AsyncHooks::fields_.
 *
 * async_uid_fields is a Float64Array() that contains the async/trigger ids for
 * several operations. These fields are as follows:
 *   kCurrentAsyncId: The async id of the current execution stack.
 *   kCurrentTriggerId: The trigger id of the current execution stack.
 *   kAsyncUidCntr: Counter that tracks the unique ids given to new resources.
 *   kInitTriggerId: Written to just before creating a new resource, so the
 *    constructor knows what other resource is responsible for its init().
 *    Used this way so the trigger id doesn't need to be passed to every
 *    resource's constructor.
 */
const { async_hook_fields, async_uid_fields } = async_wrap;
// Used to change the state of the async id stack.
const { pushAsyncIds, popAsyncIds } = async_wrap;
// Array of all AsyncHooks that will be iterated whenever an async event fires.
// Using var instead of (preferably const) in order to assign
// tmp_active_hooks_array if a hook is enabled/disabled during hook execution.
var active_hooks_array = [];
// Track whether a hook callback is currently being processed. Used to make
// sure active_hooks_array isn't altered in mid execution if another hook is
// added or removed.
var processing_hook = false;
// Use to temporarily store and updated active_hooks_array if the user enables
// or disables a hook while hooks are being processed.
var tmp_active_hooks_array = null;
// Keep track of the field counts held in tmp_active_hooks_array.
var tmp_async_hook_fields = null;

// Each constant tracks how many callbacks there are for any given step of
// async execution. These are tracked so if the user didn't include callbacks
// for a given step, that step can bail out early.
const { kInit, kBefore, kAfter, kDestroy, kCurrentAsyncId, kCurrentTriggerId,
    kAsyncUidCntr, kInitTriggerId } = async_wrap.constants;

const { async_id_symbol, trigger_id_symbol } = async_wrap;

// Used in AsyncHook and AsyncResource.
const init_symbol = Symbol('init');
const before_symbol = Symbol('before');
const after_symbol = Symbol('after');
const destroy_symbol = Symbol('destroy');

let setupHooksCalled = false;

// Used to fatally abort the process if a callback throws.
function fatalError(e) {
  if (typeof e.stack === 'string') {
    process._rawDebug(e.stack);
  } else {
    const o = { message: e };
    Error.captureStackTrace(o, fatalError);
    process._rawDebug(o.stack);
  }
  if (process.execArgv.some(
      (e) => /^--abort[_-]on[_-]uncaught[_-]exception$/.test(e))) {
    process.abort();
  }
  process.exit(1);
}


// Public API //

class AsyncHook {
  constructor({ init, before, after, destroy }) {
    if (init !== undefined && typeof init !== 'function')
      throw new TypeError('init must be a function');
    if (before !== undefined && typeof before !== 'function')
      throw new TypeError('before must be a function');
    if (after !== undefined && typeof after !== 'function')
      throw new TypeError('after must be a function');
    if (destroy !== undefined && typeof destroy !== 'function')
      throw new TypeError('destroy must be a function');

    this[init_symbol] = init;
    this[before_symbol] = before;
    this[after_symbol] = after;
    this[destroy_symbol] = destroy;
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

    if (!setupHooksCalled) {
      setupHooksCalled = true;
      // Setup the callbacks that node::AsyncWrap will call when there are
      // hooks to process. They use the same functions as the JS embedder API.
      async_wrap.setupHooks({ init,
                              before: emitBeforeN,
                              after: emitAfterN,
                              destroy: emitDestroyN });
    }

    // createHook() has already enforced that the callbacks are all functions,
    // so here simply increment the count of whether each callbacks exists or
    // not.
    hook_fields[kInit] += +!!this[init_symbol];
    hook_fields[kBefore] += +!!this[before_symbol];
    hook_fields[kAfter] += +!!this[after_symbol];
    hook_fields[kDestroy] += +!!this[destroy_symbol];
    hooks_array.push(this);
    return this;
  }

  disable() {
    const [hooks_array, hook_fields] = getHookArrays();

    const index = hooks_array.indexOf(this);
    if (index === -1)
      return this;

    hook_fields[kInit] -= +!!this[init_symbol];
    hook_fields[kBefore] -= +!!this[before_symbol];
    hook_fields[kAfter] -= +!!this[after_symbol];
    hook_fields[kDestroy] -= +!!this[destroy_symbol];
    hooks_array.splice(index, 1);
    return this;
  }
}


function getHookArrays() {
  if (!processing_hook)
    return [active_hooks_array, async_hook_fields];
  // If this hook is being enabled while in the middle of processing the array
  // of currently active hooks then duplicate the current set of active hooks
  // and store this there. This shouldn't fire until the next time hooks are
  // processed.
  if (tmp_active_hooks_array === null)
    storeActiveHooks();
  return [tmp_active_hooks_array, tmp_async_hook_fields];
}


function storeActiveHooks() {
  tmp_active_hooks_array = active_hooks_array.slice();
  // Don't want to make the assumption that kInit to kDestroy are indexes 0 to
  // 4. So do this the long way.
  tmp_async_hook_fields = [];
  tmp_async_hook_fields[kInit] = async_hook_fields[kInit];
  tmp_async_hook_fields[kBefore] = async_hook_fields[kBefore];
  tmp_async_hook_fields[kAfter] = async_hook_fields[kAfter];
  tmp_async_hook_fields[kDestroy] = async_hook_fields[kDestroy];
}


// Then restore the correct hooks array in case any hooks were added/removed
// during hook callback execution.
function restoreTmpHooks() {
  active_hooks_array = tmp_active_hooks_array;
  async_hook_fields[kInit] = tmp_async_hook_fields[kInit];
  async_hook_fields[kBefore] = tmp_async_hook_fields[kBefore];
  async_hook_fields[kAfter] = tmp_async_hook_fields[kAfter];
  async_hook_fields[kDestroy] = tmp_async_hook_fields[kDestroy];

  tmp_active_hooks_array = null;
  tmp_async_hook_fields = null;
}


function createHook(fns) {
  return new AsyncHook(fns);
}


function currentId() {
  return async_uid_fields[kCurrentAsyncId];
}


function triggerId() {
  return async_uid_fields[kCurrentTriggerId];
}


// Embedder API //

class AsyncResource {
  constructor(type, triggerId) {
    this[async_id_symbol] = ++async_uid_fields[kAsyncUidCntr];
    // Read and reset the current kInitTriggerId so that when the constructor
    // finishes the kInitTriggerId field is always 0.
    if (triggerId === undefined) {
      triggerId = initTriggerId();
    // If a triggerId was passed, any kInitTriggerId still must be null'd.
    } else {
      async_uid_fields[kInitTriggerId] = 0;
    }
    this[trigger_id_symbol] = triggerId;

    if (typeof type !== 'string' || type.length <= 0)
      throw new TypeError('type must be a string with length > 0');
    if (!Number.isSafeInteger(triggerId) || triggerId < 0)
      throw new RangeError('triggerId must be an unsigned integer');

    // Return immediately if there's nothing to do.
    if (async_hook_fields[kInit] === 0)
      return;

    init(this[async_id_symbol], type, triggerId, this);
  }

  emitBefore() {
    emitBeforeS(this[async_id_symbol], this[trigger_id_symbol]);
    return this;
  }

  emitAfter() {
    emitAfterS(this[async_id_symbol]);
    return this;
  }

  emitDestroy() {
    emitDestroyS(this[async_id_symbol]);
    return this;
  }

  asyncId() {
    return this[async_id_symbol];
  }

  triggerId() {
    return this[trigger_id_symbol];
  }
}


function runInAsyncIdScope(asyncId, cb) {
  // Store the async id now to make sure the stack is still good when the ids
  // are popped off the stack.
  const prevId = currentId();
  pushAsyncIds(asyncId, prevId);
  try {
    cb();
  } finally {
    popAsyncIds(asyncId);
  }
}


// Sensitive Embedder API //

// Increment the internal id counter and return the value. Important that the
// counter increment first. Since it's done the same way in
// Environment::new_async_uid()
function newUid() {
  return ++async_uid_fields[kAsyncUidCntr];
}


// Return the triggerId meant for the constructor calling it. It's up to the
// user to safeguard this call and make sure it's zero'd out when the
// constructor is complete.
function initTriggerId() {
  var tId = async_uid_fields[kInitTriggerId];
  // Reset value after it's been called so the next constructor doesn't
  // inherit it by accident.
  async_uid_fields[kInitTriggerId] = 0;
  if (tId <= 0)
    tId = async_uid_fields[kCurrentAsyncId];
  return tId;
}


function setInitTriggerId(triggerId) {
  // CHECK(Number.isSafeInteger(triggerId))
  // CHECK(triggerId > 0)
  async_uid_fields[kInitTriggerId] = triggerId;
}


function emitInitS(asyncId, type, triggerId, resource) {
  // Short circuit all checks for the common case. Which is that no hooks have
  // been set. Do this to remove performance impact for embedders (and core).
  // Even though it bypasses all the argument checks. The performance savings
  // here is critical.
  if (async_hook_fields[kInit] === 0)
    return;

  // This can run after the early return check b/c running this function
  // manually means that the embedder must have used initTriggerId().
  if (!Number.isSafeInteger(triggerId)) {
    if (triggerId !== undefined)
      resource = triggerId;
    triggerId = initTriggerId();
  }

  // I'd prefer allowing these checks to not exist, or only throw in a debug
  // build, in order to improve performance.
  if (!Number.isSafeInteger(asyncId) || asyncId < 0)
    throw new RangeError('asyncId must be an unsigned integer');
  if (typeof type !== 'string' || type.length <= 0)
    throw new TypeError('type must be a string with length > 0');
  if (!Number.isSafeInteger(triggerId) || triggerId < 0)
    throw new RangeError('triggerId must be an unsigned integer');

  init(asyncId, type, triggerId, resource);

  // Isn't null if hooks were added/removed while the hooks were running.
  if (tmp_active_hooks_array !== null) {
    restoreTmpHooks();
  }
}


function emitBeforeN(asyncId) {
  processing_hook = true;
  // Use a single try/catch for all hook to avoid setting up one per iteration.
  try {
    for (var i = 0; i < active_hooks_array.length; i++) {
      if (typeof active_hooks_array[i][before_symbol] === 'function') {
        active_hooks_array[i][before_symbol](asyncId);
      }
    }
  } catch (e) {
    fatalError(e);
  }
  processing_hook = false;

  if (tmp_active_hooks_array !== null) {
    restoreTmpHooks();
  }
}


// Usage: emitBeforeS(asyncId[, triggerId]). If triggerId is omitted then
// asyncId will be used instead.
function emitBeforeS(asyncId, triggerId = asyncId) {
  // CHECK(Number.isSafeInteger(asyncId) && asyncId > 0)
  // CHECK(Number.isSafeInteger(triggerId) && triggerId > 0)

  // Validate the ids.
  if (asyncId < 0 || triggerId < 0) {
    fatalError('before(): asyncId or triggerId is less than zero ' +
               `(asyncId: ${asyncId}, triggerId: ${triggerId})`);
  }

  pushAsyncIds(asyncId, triggerId);

  if (async_hook_fields[kBefore] === 0)
    return;
  emitBeforeN(asyncId);
}


// Called from native. The asyncId stack handling is taken care of there before
// this is called.
function emitAfterN(asyncId) {
  processing_hook = true;
  // Use a single try/catch for all hook to avoid setting up one per iteration.
  try {
    for (var i = 0; i < active_hooks_array.length; i++) {
      if (typeof active_hooks_array[i][after_symbol] === 'function') {
        active_hooks_array[i][after_symbol](asyncId);
      }
    }
  } catch (e) {
    fatalError(e);
  }
  processing_hook = false;

  if (tmp_active_hooks_array !== null) {
    restoreTmpHooks();
  }
}


// TODO(trevnorris): Calling emitBefore/emitAfter from native can't adjust the
// kIdStackIndex. But what happens if the user doesn't have both before and
// after callbacks.
function emitAfterS(asyncId) {
  if (async_hook_fields[kAfter] > 0)
    emitAfterN(asyncId);

  popAsyncIds(asyncId);
}


function emitDestroyS(asyncId) {
  // Return early if there are no destroy callbacks, or on attempt to emit
  // destroy on the void.
  if (async_hook_fields[kDestroy] === 0 || asyncId === 0)
    return;
  async_wrap.addIdToDestroyList(asyncId);
}


function emitDestroyN(asyncId) {
  processing_hook = true;
  // Use a single try/catch for all hook to avoid setting up one per iteration.
  try {
    for (var i = 0; i < active_hooks_array.length; i++) {
      if (typeof active_hooks_array[i][destroy_symbol] === 'function') {
        active_hooks_array[i][destroy_symbol](asyncId);
      }
    }
  } catch (e) {
    fatalError(e);
  }
  processing_hook = false;

  if (tmp_active_hooks_array !== null) {
    restoreTmpHooks();
  }
}


// Emit callbacks for native calls. Since some state can be setup directly from
// C++ there's no need to perform all the work here.

// This should only be called if hooks_array has kInit > 0. There are no global
// values to setup. Though hooks_array will be cloned if C++ needed to call
// init().
// TODO(trevnorris): Perhaps have MakeCallback call a single JS function that
// does the before/callback/after calls to remove two additional calls to JS.

// Force the application to shutdown if one of the callbacks throws. This may
// change in the future depending on whether it can be determined if there's a
// slim chance of the application remaining stable after handling one of these
// exceptions.
function init(asyncId, type, triggerId, resource) {
  processing_hook = true;
  // Use a single try/catch for all hook to avoid setting up one per iteration.
  try {
    for (var i = 0; i < active_hooks_array.length; i++) {
      if (typeof active_hooks_array[i][init_symbol] === 'function') {
        active_hooks_array[i][init_symbol](asyncId, type, triggerId, resource);
      }
    }
  } catch (e) {
    fatalError(e);
  }
  processing_hook = false;
}


// Placing all exports down here because the exported classes won't export
// otherwise.
module.exports = {
  // Public API
  createHook,
  currentId,
  triggerId,
  // Embedder API
  AsyncResource,
  runInAsyncIdScope,
  // Sensitive Embedder API
  newUid,
  initTriggerId,
  setInitTriggerId,
  emitInit: emitInitS,
  emitBefore: emitBeforeS,
  emitAfter: emitAfterS,
  emitDestroy: emitDestroyS,
};
