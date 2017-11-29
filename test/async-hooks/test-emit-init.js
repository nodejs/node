'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const async_hooks = require('internal/async_hooks');
const initHooks = require('./init-hooks');

const expectedId = async_hooks.newUid();
const expectedTriggerId = async_hooks.newUid();
const expectedType = 'test_emit_init_type';
const expectedResource = { key: 'test_emit_init_resource' };

const hooks1 = initHooks({
  oninit: common.mustCall((id, type, triggerAsyncId, resource) => {
    assert.strictEqual(id, expectedId);
    assert.strictEqual(type, expectedType);
    assert.strictEqual(triggerAsyncId, expectedTriggerId);
    assert.strictEqual(resource.key, expectedResource.key);
  })
});

hooks1.enable();

switch (process.argv[2]) {
  case 'test_invalid_async_id':
    async_hooks.emitInit();
    return;
  case 'test_invalid_trigger_id':
    async_hooks.emitInit(expectedId);
    return;
  case 'test_invalid_trigger_id_negative':
    async_hooks.emitInit(expectedId, expectedType, -2);
    return;
}
assert.ok(!process.argv[2]);


const c1 = spawnSync(process.execPath, [
  '--expose-internals', __filename, 'test_invalid_async_id'
]);
assert.strictEqual(
  c1.stderr.toString().split(/[\r\n]+/g)[0],
  'RangeError [ERR_INVALID_ASYNC_ID]: Invalid asyncId value: undefined');
assert.strictEqual(c1.status, 1);

const c2 = spawnSync(process.execPath, [
  '--expose-internals', __filename, 'test_invalid_trigger_id'
]);
assert.strictEqual(
  c2.stderr.toString().split(/[\r\n]+/g)[0],
  'RangeError [ERR_INVALID_ASYNC_ID]: Invalid triggerAsyncId value: undefined');
assert.strictEqual(c2.status, 1);

const c3 = spawnSync(process.execPath, [
  '--expose-internals', __filename, 'test_invalid_trigger_id_negative'
]);
assert.strictEqual(
  c3.stderr.toString().split(/[\r\n]+/g)[0],
  'RangeError [ERR_INVALID_ASYNC_ID]: Invalid triggerAsyncId value: -2');
assert.strictEqual(c3.status, 1);


async_hooks.emitInit(expectedId, expectedType, expectedTriggerId,
                     expectedResource);

hooks1.disable();

initHooks({
  oninit: common.mustCall((id, type, triggerAsyncId, resource) => {
    assert.strictEqual(id, expectedId);
    assert.strictEqual(type, expectedType);
    assert.notStrictEqual(triggerAsyncId, expectedTriggerId);
    assert.strictEqual(resource.key, expectedResource.key);
  })
}).enable();

async_hooks.emitInit(expectedId, expectedType, null, expectedResource);
