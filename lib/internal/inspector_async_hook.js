'use strict';

const {
  ArrayPrototypeIncludes,
  ArrayPrototypeIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSplice,
} = primordials;

const {
  getHookArrays,
  enableHooks,
  disableHooks,
  symbols: {
    init_symbol,
    before_symbol,
    after_symbol,
    destroy_symbol,
  },
  constants: {
    kInit,
    kBefore,
    kAfter,
    kDestroy,
    kTotals,
  },
  kNoPromiseHook,
} = require('internal/async_hooks');

let hook;
let config;

function lazyHookCreation() {
  const inspector = internalBinding('inspector');
  config = internalBinding('config');

  hook = {
    [init_symbol](asyncId, type, triggerAsyncId, resource) {
    // It's difficult to tell which tasks will be recurring and which won't,
    // therefore we mark all tasks as recurring. Based on the discussion
    // in https://github.com/nodejs/node/pull/13870#discussion_r124515293,
    // this should be fine as long as we call asyncTaskCanceled() too.
      const recurring = true;
      inspector.asyncTaskScheduled(type, asyncId, recurring);
    },

    [before_symbol](asyncId) {
      inspector.asyncTaskStarted(asyncId);
    },

    [after_symbol](asyncId) {
      inspector.asyncTaskFinished(asyncId);
    },

    [destroy_symbol](asyncId) {
      inspector.asyncTaskCanceled(asyncId);
    },

    // The inspector does not want promise hook callbacks.
    [kNoPromiseHook]: true,

    enable() {
      const { 0: hooks_array, 1: hook_fields } = getHookArrays();

      // Each hook is only allowed to be added once.
      if (ArrayPrototypeIncludes(hooks_array, this))
        return;

      // This hook always registers all four callbacks, so each count is
      // incremented exactly once.
      const prev_kTotals = hook_fields[kTotals];
      hook_fields[kTotals] = hook_fields[kInit] += 1;
      hook_fields[kTotals] += hook_fields[kBefore] += 1;
      hook_fields[kTotals] += hook_fields[kAfter] += 1;
      hook_fields[kTotals] += hook_fields[kDestroy] += 1;
      ArrayPrototypePush(hooks_array, this);

      if (prev_kTotals === 0 && hook_fields[kTotals] > 0)
        enableHooks();
    },

    disable() {
      const { 0: hooks_array, 1: hook_fields } = getHookArrays();

      const index = ArrayPrototypeIndexOf(hooks_array, this);
      if (index === -1)
        return;

      const prev_kTotals = hook_fields[kTotals];
      hook_fields[kTotals] = hook_fields[kInit] -= 1;
      hook_fields[kTotals] += hook_fields[kBefore] -= 1;
      hook_fields[kTotals] += hook_fields[kAfter] -= 1;
      hook_fields[kTotals] += hook_fields[kDestroy] -= 1;
      ArrayPrototypeSplice(hooks_array, index, 1);

      if (prev_kTotals > 0 && hook_fields[kTotals] === 0)
        disableHooks();
    },
  };
}

function enable() {
  if (hook === undefined) lazyHookCreation();
  if (config.bits < 64) {
    // V8 Inspector stores task ids as (void*) pointers.
    // async_hooks store ids as 64bit numbers.
    // As a result, we cannot reliably translate async_hook ids to V8 async_task
    // ids on 32bit platforms.
    process.emitWarning(
      'Warning: Async stack traces in debugger are not available ' +
      `on ${config.bits}bit platforms. The feature is disabled.`,
      {
        code: 'INSPECTOR_ASYNC_STACK_TRACES_NOT_AVAILABLE',
      });
  } else {
    hook.enable();
  }
}

function disable() {
  if (hook === undefined) lazyHookCreation();
  hook.disable();
}

module.exports = {
  enable,
  disable,
};
