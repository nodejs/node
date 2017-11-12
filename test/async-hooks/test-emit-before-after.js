'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const async_hooks = require('internal/async_hooks');
const initHooks = require('./init-hooks');

switch (process.argv[2]) {
  case 'test_invalid_async_id':
    async_hooks.emitBefore(-2, 1);
    return;
  case 'test_invalid_trigger_id':
    async_hooks.emitBefore(1, -2);
    return;
}
assert.ok(!process.argv[2]);


const c1 = spawnSync(process.execPath, [
  '--expose-internals', __filename, 'test_invalid_async_id'
]);
assert.strictEqual(
  c1.stderr.toString().split(/[\r\n]+/g)[0],
  'RangeError [ERR_INVALID_ASYNC_ID]: Invalid asyncId value: -2');
assert.strictEqual(c1.status, 1);

const c2 = spawnSync(process.execPath, [
  '--expose-internals', __filename, 'test_invalid_trigger_id'
]);
assert.strictEqual(
  c2.stderr.toString().split(/[\r\n]+/g)[0],
  'RangeError [ERR_INVALID_ASYNC_ID]: Invalid triggerAsyncId value: -2');
assert.strictEqual(c2.status, 1);

const expectedId = async_hooks.newUid();
const expectedTriggerId = async_hooks.newUid();
const expectedType = 'test_emit_before_after_type';

// Verify that if there is no registered hook, then nothing will happen.
async_hooks.emitBefore(expectedId, expectedTriggerId);
async_hooks.emitAfter(expectedId);

initHooks({
  onbefore: common.mustCall((id) => assert.strictEqual(id, expectedId)),
  onafter: common.mustCall((id) => assert.strictEqual(id, expectedId)),
  allowNoInit: true
}).enable();

async_hooks.emitInit(expectedId, expectedType, expectedTriggerId);
async_hooks.emitBefore(expectedId, expectedTriggerId);
async_hooks.emitAfter(expectedId);
