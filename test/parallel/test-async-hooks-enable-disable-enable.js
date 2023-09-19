'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

// Regression test for https://github.com/nodejs/node/issues/27585.

async_hooks.createHook({ init: () => {} }).enable().disable().enable();
async_hooks.createHook({ init: () => {} }).enable();

async function main() {
  const initialAsyncId = async_hooks.executionAsyncId();
  await 0;
  assert.notStrictEqual(async_hooks.executionAsyncId(), initialAsyncId);
}

main().then(common.mustCall());
