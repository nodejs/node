'use strict';

const common = require('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const TIMEOUT = common.platformTimeout(100);

const hooks = initHooks();
hooks.enable();

// install first timeout
setInterval(common.mustCall(ontimeout), TIMEOUT);
const as = hooks.activitiesOfTypes('Timeout');
assert.strictEqual(as.length, 1);
const t1 = as[0];
assert.strictEqual(t1.type, 'Timeout');
assert.strictEqual(typeof t1.uid, 'number');
assert.strictEqual(typeof t1.triggerAsyncId, 'number');
checkInvocations(t1, { init: 1 }, 't1: when timer installed');

function ontimeout() {
  checkInvocations(t1, { init: 1, before: 1 }, 't1: when first timer fired');

  throw new Error('setInterval Error');
}

process.on('uncaughtException', common.mustCall((err) => {
  assert(err.message, 'setInterval Error');
}));

process.on('exit', () => {
  hooks.disable();
  hooks.sanityCheck('Timeout');

  checkInvocations(t1, { init: 1, before: 1, after: 1, destroy: 1 },
                   't1: when process exits');
});
