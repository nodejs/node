'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const TIMEOUT = common.platformTimeout(100);

const hooks = initHooks();
hooks.enable();

// install first timeout
setTimeout(common.mustCall(ontimeout), TIMEOUT);
const as = hooks.activitiesOfTypes('TIMERWRAP');
assert.strictEqual(as.length, 1);
const t1 = as[0];
assert.strictEqual(t1.type, 'TIMERWRAP');
assert.strictEqual(typeof t1.uid, 'number');
assert.strictEqual(typeof t1.triggerAsyncId, 'number');
checkInvocations(t1, { init: 1 }, 't1: when first timer installed');

function ontimeout() {
  checkInvocations(t1, { init: 1, before: 1 }, 't1: when first timer fired');

  setTimeout(onsecondTimeout, TIMEOUT);
  const as = hooks.activitiesOfTypes('TIMERWRAP');
  assert.strictEqual(as.length, 1);
  checkInvocations(t1, { init: 1, before: 1 },
                   't1: when second timer installed');
}

function onsecondTimeout() {
  const as = hooks.activitiesOfTypes('TIMERWRAP');
  assert.strictEqual(as.length, 1);
  checkInvocations(t1, { init: 1, before: 2, after: 1 },
                   't1: when second timer fired');

  // install third timeout with different TIMEOUT
  setTimeout(onthirdTimeout, TIMEOUT + 1);
  checkInvocations(t1, { init: 1, before: 2, after: 1 },
                   't1: when third timer installed');
}

function onthirdTimeout() {
  checkInvocations(t1, { init: 1, before: 3, after: 2 },
                   't1: when third timer fired');
  tick(2);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('TIMERWRAP');

  checkInvocations(t1, { init: 1, before: 3, after: 3 },
                   't1: when process exits');
}
