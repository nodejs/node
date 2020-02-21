'use strict';
const common = require('../common');

const assert = require('assert');
const async_hooks = require('async_hooks');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();
hooks.enable();

const rootAsyncId = async_hooks.executionAsyncId();

queueMicrotask(common.mustCall(() => {
  assert.strictEqual(async_hooks.triggerAsyncId(), rootAsyncId);
}));

process.on('exit', () => {
  hooks.sanityCheck();

  const as = hooks.activitiesOfTypes('Microtask');
  checkInvocations(as[0], {
    init: 1, before: 1, after: 1, destroy: 1
  }, 'when process exits');
});
