'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

const asyncId = async_hooks.newUid();

assert.notStrictEqual(async_hooks.executionAsyncId(), asyncId);

async_hooks.runInAsyncIdScope(asyncId, common.mustCall(() => {
  assert.strictEqual(async_hooks.executionAsyncId(), asyncId);
}));
