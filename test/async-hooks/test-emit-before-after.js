'use strict';
// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const async_hooks = require('internal/async_hooks');
const initHooks = require('./init-hooks');

const expectedId = async_hooks.newAsyncId();
const expectedTriggerId = async_hooks.newAsyncId();
const expectedType = 'test_emit_before_after_type';

// Verify that if there is no registered hook, then nothing will happen.
async_hooks.emitBefore(expectedId, expectedTriggerId);
async_hooks.emitAfter(expectedId);

const chkBefore = common.mustCall((id) => assert.strictEqual(id, expectedId));
const chkAfter = common.mustCall((id) => assert.strictEqual(id, expectedId));

const checkOnce = (fn) => {
  let called = false;
  return (...args) => {
    if (called) return;

    called = true;
    fn(...args);
  };
};

initHooks({
  onbefore: checkOnce(chkBefore),
  onafter: checkOnce(chkAfter),
  allowNoInit: true
}).enable();

async_hooks.emitInit(expectedId, expectedType, expectedTriggerId);
async_hooks.emitBefore(expectedId, expectedTriggerId);
async_hooks.emitAfter(expectedId);
