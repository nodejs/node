'use strict';

const common = require('../common');
const assert = require('assert');
const { createHook } = require('async_hooks');

// async_hooks.createHook is disabled by default (without --allow-async-hooks)
{
  assert.throws(() => {
    createHook({ init() {} });
  }, common.expectsError({
    code: 'ERR_ASYNC_HOOKS_CREATE_HOOK_DISABLED',
    message: 'async_hooks.createHook() is disabled. Use --allow-async-hooks to enable it.',
  }));
}
