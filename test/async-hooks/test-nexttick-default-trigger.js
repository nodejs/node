'use strict';
const common = require('../common');

// This tests ensures that the triggerId of the nextTick function sets the
// triggerAsyncId correctly.

const assert = require('assert');
const async_hooks = require('async_hooks');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();
hooks.enable();

const rootAsyncId = async_hooks.executionAsyncId();

process.nextTick(common.mustCall(function() {
  assert.strictEqual(async_hooks.triggerAsyncId(), rootAsyncId);
}));

process.on('exit', function() {
  hooks.sanityCheck();

  const as = hooks.activitiesOfTypes('TickObject');
  checkInvocations(as[0], {
    init: 1, before: 1, after: 1, destroy: 1
  }, 'when process exits');
});
