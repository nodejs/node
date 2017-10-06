'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const initHooks = require('./init-hooks');

// Verify that if there is no registered hook, then those invalid parameters
// won't be checked.
assert.doesNotThrow(() => async_hooks.emitInit());

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

assert.throws(() => {
  async_hooks.emitInit();
}, common.expectsError({
  code: 'ERR_INVALID_ASYNC_ID',
  type: RangeError,
}));
assert.throws(() => {
  async_hooks.emitInit(expectedId);
}, common.expectsError({
  code: 'ERR_INVALID_ASYNC_ID',
  type: RangeError,
}));
assert.throws(() => {
  async_hooks.emitInit(expectedId, expectedType, -2);
}, common.expectsError({
  code: 'ERR_INVALID_ASYNC_ID',
  type: RangeError,
}));

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
