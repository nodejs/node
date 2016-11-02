'use strict';

const async_wrap = process.binding('async_wrap');
// A Uint32Array wrapping node::Environment::AsyncHooks::fields_. An array that
// communicates between JS and C++ how many of a given type of callback are
// available to be called.
const async_hook_fields = async_wrap.async_hook_fields;
// A Float64Array wrapping node::Environment::AsyncHooks::uid_fields_. An array
// that communicates the state of the currentId and triggerId. Fields are as
// follows:
//   kAsyncUidCntr: Maintain state of next unique id.
//   kCurrentId: Read/write the id of the current execution context.
//   kTriggerId: Read/write the id of the resource responsible for the current
//    execution context firing.
//   kInitTriggerId: Written to just before creating a new resource, so the
//    constructor knows what other resource is responsible for its init().
//   kScopedTriggerId: Hold the init triggerId for all constructors that run
//    within triggerIdScope().
const async_uid_fields = async_wrap.async_uid_fields;
// Array of all AsyncHooks that will be iterated whenever an async event fires.
const active_hooks_array = [];
// Stack of scoped trigger id's for triggerIdScope()
const trigger_scope_stack = [];
// Array that holds the current and trigger id's for between before/after.
const current_trigger_id_stack = [];

const async_id_symbol = Symbol('_asyncId');
const trigger_id_symbol = Symbol('_triggerId');
const init_symbol = Symbol('init');
const before_symbol = Symbol('before');
const after_symbol = Symbol('after');
const destroy_symbol = Symbol('destroy');

// Each constant tracks how many callbacks there are for any given step of
// async execution. These are tracked so if the user didn't include callbacks
// for a given step, that step can bail out early.
// The exception is kActiveHooks. Which tracks the total number of AsyncEvents
// that exist on "active_hooks_array".
const { kInit, kBefore, kAfter, kDestroy, kActiveHooks, kAsyncUidCntr,
        kCurrentId, kTriggerId, kScopedTriggerId, kInitTriggerId } =
            async_wrap.constants;

// Setup the callbacks that node::AsyncWrap will call when there are hooks to
// process. They use the same functions as the JS embedder API.
async_wrap.setupHooks({ init,
                        before: emitBeforeS,
                        after: emitAfterS,
                        destroy: emitDestroyS });


// Public API //

class AsyncHook {
  constructor(fns) {
    this[init_symbol] = fns.init;
    this[before_symbol] = fns.before;
    this[after_symbol] = fns.after;
    this[destroy_symbol] = fns.destroy;
  }

  // TODO(trevnorris): Both enable and disable need to check if 1) callbacks
  // are in the middle of being processed or 2) async destroy's are queue'd
  // to be called. In either case a copy of active_hooks_array needs to be
  // duplicated then replaced with the altered array once the operation is
  // complete.
  enable() {
    if (active_hooks_array.indexOf(this) !== -1)
      return;
    // createHook() has already enforced that the callbacks are all functions,
    // so here simply increment the count of whether each callbacks exists or
    // not.
    async_hook_fields[kInit] += +!!this[init_symbol];
    async_hook_fields[kBefore] += +!!this[before_symbol];
    async_hook_fields[kAfter] += +!!this[after_symbol];
    async_hook_fields[kDestroy] += +!!this[destroy_symbol];
    async_hook_fields[kActiveHooks]++;
    active_hooks_array.push(this);
    return this;
  }

  disable() {
    const index = active_hooks_array.indexOf(this);
    if (index === -1)
      return;
    async_hook_fields[kInit] -= +!!this[init_symbol];
    async_hook_fields[kBefore] -= +!!this[before_symbol];
    async_hook_fields[kAfter] -= +!!this[after_symbol];
    async_hook_fields[kDestroy] -= +!!this[destroy_symbol];
    async_hook_fields[kActiveHooks]--;
    active_hooks_array.splice(index, 1);
    return this;
  }
}


function createHook(fns) {
  if (fns.init !== undefined && typeof fns.init !== 'function')
    throw new TypeError('init must be a function');
  if (fns.before !== undefined && typeof fns.before !== 'function')
    throw new TypeError('before must be a function');
  if (fns.after !== undefined && typeof fns.after !== 'function')
    throw new TypeError('after must be a function');
  if (fns.destroy !== undefined && typeof fns.destroy !== 'function')
    throw new TypeError('destroy must be a function');

  return new AsyncHook(fns);
}


function currentId() {
  return async_uid_fields[kCurrentId];
}


function triggerId() {
  return async_uid_fields[kTriggerId];
}


// Embedder API //

class AsyncEvent {
  constructor(type, triggerId) {
    this[async_id_symbol] = ++async_uid_fields[kAsyncUidCntr];
    // Do this before early return to null any potential kInitTriggerId.
    if (triggerId === undefined)
      triggerId = initTriggerId();
    // If a triggerId was passed, any kInitTriggerId still must be null'd.
    else
      async_uid_fields[kInitTriggerId] = 0;
    this[trigger_id_symbol] = triggerId;

    // Return immediately if there's nothing to do. Checking for kInit covers
    // checking for kActiveHooks. Doing this before the checks for performance.
    if (async_hook_fields[kInit] === 0)
      return;

    if (typeof type !== 'string' || type.length <= 0)
      throw new TypeError('type must be a string with length > 0');
    if (!Number.isSafeInteger(triggerId) || triggerId < 0)
      throw new RangeError('triggerId must be an unsigned integer');

    for (var i = 0; i < active_hooks_array.length; i++) {
      if (typeof active_hooks_array[i][init_symbol] === 'function') {
        runInitCallback(active_hooks_array[i][init_symbol],
                        this[async_id_symbol],
                        type,
                        triggerId,
                        this);
      }
    }
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


function triggerIdScope(id, cb) {
  if (async_uid_fields[kScopedTriggerId] > 0)
    trigger_scope_stack.push(async_uid_fields[kScopedTriggerId]);
  try {
    cb();
  } finally {
    if (trigger_scope_stack.length > 0)
      trigger_scope_stack.pop();
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
  if (tId <= 0) tId = async_uid_fields[kScopedTriggerId];
  // Reset value after it's been called so the next constructor doesn't
  // inherit it by accident.
  else async_uid_fields[kInitTriggerId] = 0;
  if (tId < 0) tId = async_uid_fields[kCurrentId];
  return tId;
}


function setInitTriggerId(id) {
  async_uid_fields[kInitTriggerId] = id;
}


// Usage:
//    emitInit(number, string[, number[, handle]])
//
// Short circuit all checks for the common case. Which is that no hooks have
// been set. Do this to remove performance impact for embedders (and core).
// Even though it bypasses all the argument checks. The performance savings
// here is critical.
//
// Note: All the emit* below end with "S" so they can be called by the methods
// on AsyncEvent, but are not exported that way.
function emitInitS(id, type, triggerId, handle) {
  // Return immediately if there's nothing to do. Checking for kInit covers
  // checking for kActiveHooks. Doing this before the checks for performance.
  if (async_hook_fields[kInit] === 0)
    return;

  // This can run after the early return check b/c running this function
  // manually means that the embedder must have used initTriggerId().
  if (triggerId === undefined)
    triggerId = initTriggerId();

  // I'd prefer allowing these checks to not exist, or only throw in a debug
  // build, to improve performance.
  if (!Number.isSafeInteger(id) || id < 0)
    throw new RangeError('id must be an unsigned integer');
  if (typeof type !== 'string' || type.length <= 0)
    throw new TypeError('type must be a string with length > 0');
  if (!Number.isSafeInteger(triggerId) || triggerId < 0)
    throw new RangeError('triggerId must be an unsigned integer');

  for (var i = 0; i < active_hooks_array.length; i++) {
    if (typeof active_hooks_array[i][init_symbol] === 'function') {
      runInitCallback(
        active_hooks_array[i][init_symbol], id, type, triggerId, handle);
    }
  }
}


// Usage: emitBeforeS(id[, triggerId]). If triggerId is omitted then id will be
// used instead.
function emitBeforeS(id, triggerId) {
  // First setup the currentId and triggerId for the coming callback.
  const currentCurrentId = async_uid_fields[kCurrentId];
  const currentTriggerId = async_uid_fields[kTriggerId];
  if (currentCurrentId > 0 || currentTriggerId > 0 ||
      current_trigger_id_stack.length > 0) {
    current_trigger_id_stack.push(currentCurrentId, currentTriggerId);
  }
  async_uid_fields[kCurrentId] = id;
  async_uid_fields[kTriggerId] = triggerId === undefined ? id : triggerId;

  if (active_hooks_array[kBefore] === 0) {
    return;
  }
  for (var i = 0; i < active_hooks_array.length; i++) {
    if (typeof active_hooks_array[i][before_symbol] === 'function') {
      runCallback(active_hooks_array[i][before_symbol], id);
    }
  }
}


function emitAfterS(id) {
  // Remove state after the call has completed.
  // TODO(trevnorris): Clearing the native counters is easy, but also must
  // track the length of the array manually so the native side can empty it
  // in case there's an error. Or, export the array in such a way that
  // fatalException can empty it as well.
  if (current_trigger_id_stack.length > 0) {
    async_uid_fields[kTriggerId] = current_trigger_id_stack.pop();
    async_uid_fields[kCurrentId] = current_trigger_id_stack.pop();
  } else {
    async_uid_fields[kTriggerId] = 0;
    async_uid_fields[kCurrentId] = 0;
  }

  if (active_hooks_array[kAfter] === 0) {
    return;
  }
  for (var i = 0; i < active_hooks_array.length; i++) {
    if (typeof active_hooks_array[i][after_symbol] === 'function') {
      runCallback(active_hooks_array[i][after_symbol], id);
    }
  }
}


// TODO(trevnorris): If these are delayed until ran then we will have a list
// of id's that may need to run. So instead of forcing native to call into JS
// for every handle, instead have this call back into C++ for the next id.
function emitDestroyS(id) {
  if (active_hooks_array[kDestroy] === 0) {
    return;
  }
  for (var i = 0; i < active_hooks_array.length; i++) {
    if (typeof active_hooks_array[i][destroy_symbol] === 'function') {
      runCallback(active_hooks_array[i][destroy_symbol], id);
    }
  }
}


// Emit callbacks for native calls. Since some state can be setup directly from
// C++ there's no need to perform all the work here.

// This should only be called if hooks_array has kInit > 0. There are no global
// values to setup. Though hooks_array will be cloned if C++ needed to visit
// here since it's faster to do in JS.
// TODO(trevnorris): Perhaps have MakeCallback call a single JS function that
// does the before/callback/after calls to remove two additional calls to JS.
function init(id, type, handle) {
  // Use initTriggerId() here so that native versions of initTriggerId() and
  // triggerIdScope(), or so any triggerId set in JS propagates properly.
  const triggerId = initTriggerId();
  for (var i = 0; i < active_hooks_array.length; i++) {
    if (typeof active_hooks_array[i][init_symbol] === 'function') {
      runInitCallback(
        active_hooks_array[i][init_symbol], id, type, triggerId, handle);
    }
  }
}


// Generalized caller for all callbacks that handles error handling.

function runInitCallback(cb, id, type, triggerId, handle) {
  var fail = true;
  try {
    cb(id, type, triggerId, handle);
    fail = false;
  } finally {
    // Force the application to shutdown if one of the callbacks throws. This
    // may change in the future depending on whether it can be determined if
    // there's a slim chance of the application remaining stable after handling
    // one of these exceptions.
    if (fail) {
      process._events.uncaughtException = undefined;
      process.domain = undefined;
    }
  }
}


function runCallback(cb, id) {
  var fail = true;
  try {
    cb(id);
    fail = false;
  } finally {
    // Force the application to shutdown if one of the callbacks throws. This
    // may change in the future depending on whether it can be determined if
    // there's a slim chance of the application remaining stable after handling
    // one of these exceptions.
    if (fail) {
      process._events.uncaughtException = undefined;
      process.domain = undefined;
    }
  }
}


// Placing all exports down here because the exported classes won't export
// otherwise.
module.exports = {
  // Public API
  createHook,
  currentId,
  triggerId,
  // Embedder API
  AsyncEvent,
  triggerIdScope,
  // Sensitive Embedder API
  newUid,
  initTriggerId,
  setInitTriggerId,
  emitInit: emitInitS,
  emitBefore: emitBeforeS,
  emitAfter: emitAfterS,
  emitDestroy: emitDestroyS,
};
