'use strict';

const common = require('../common');

const async_hooks = require('async_hooks');
const assert = require('assert');

common.crashOnUnhandledRejection();

const asyncIds = [];

async_hooks.createHook({
  init: (asyncId, type, triggerAsyncId, resource) => {
    asyncIds.push([triggerAsyncId, asyncId]);
  }
}).enable();

async function main() {
  console.log('hello');
  await null;
  console.log('world');
}

main().then(() => {
  // Verify correct relationship between ids, e.g.:
  // '1 => 3', '1 => 4', '1 => 5', '3 => 6', '3 => 7'
  assert.strictEqual(asyncIds[0][0], asyncIds[1][0]);
  assert.strictEqual(asyncIds[0][0], asyncIds[2][0]);
  assert.strictEqual(asyncIds[0][1], asyncIds[3][0]);
  assert.strictEqual(asyncIds[0][1], asyncIds[4][0]);
  assert.strictEqual(asyncIds[0][1] + 1, asyncIds[1][1]);
  assert.strictEqual(asyncIds[0][1] + 2, asyncIds[2][1]);
  assert.strictEqual(asyncIds[3][1] + 1, asyncIds[4][1]);
});
