// Flags: --expose-internals
'use strict';

// The test runner tracks which test owns the current async context with
// AsyncLocalStorage, so it must not install any async_hooks.

const common = require('../common');
const assert = require('assert');
const { setTimeout } = require('timers/promises');
const { enabledHooksExist } = require('internal/async_hooks');
const { test } = require('node:test');

test('no async hooks are enabled', common.mustCall(async (t) => {
  assert.strictEqual(enabledHooksExist(), false);

  await t.test('subtest', common.mustCall(async () => {
    await setTimeout(1);
    assert.strictEqual(enabledHooksExist(), false);
  }));

  assert.strictEqual(enabledHooksExist(), false);
}));
