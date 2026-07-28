'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const async_hooks = require('internal/async_hooks');
const initHooks = require('./init-hooks');

const expectedId = async_hooks.newAsyncId();
const expectedTriggerId = async_hooks.newAsyncId();
const expectedType = 'test_emit_init_type';
const expectedResource = { key: 'test_emit_init_resource' };

const hooks1 = initHooks({
  oninit: common.mustCall((id, type, triggerAsyncId, resource) => {
    assert.strictEqual(id, expectedId);
    assert.strictEqual(type, expectedType);
    assert.strictEqual(triggerAsyncId, expectedTriggerId);
    assert.strictEqual(resource.key, expectedResource.key);
  }),
});

hooks1.enable();

async_hooks.emitInit(expectedId, expectedType, expectedTriggerId,
                     expectedResource);

hooks1.disable();

initHooks({
  oninit: common.mustCall((id, type, triggerAsyncId, resource) => {
    assert.strictEqual(id, expectedId);
    assert.strictEqual(type, expectedType);
    assert.notStrictEqual(triggerAsyncId, expectedTriggerId);
    assert.strictEqual(resource.key, expectedResource.key);
  }),
}).enable();

async_hooks.emitInit(expectedId, expectedType, null, expectedResource);
