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

  // install second timeout with same TIMEOUT to see timer wrap being reused
  setTimeout(onsecondTimeout, TIMEOUT);
  const as = hooks.activitiesOfTypes('TIMERWRAP');
  assert.strictEqual(as.length, 1);
  checkInvocations(t1, { init: 1, before: 1 },
                   't1: when second timer installed');
}

// even though we install 3 timers we only have two timerwrap resources created
// as one is reused for the two timers with the same timeout
let t2;

function onsecondTimeout() {
  let as = hooks.activitiesOfTypes('TIMERWRAP');
  assert.strictEqual(as.length, 1);
  checkInvocations(t1, { init: 1, before: 2, after: 1 },
                   't1: when second timer fired');

  // install third timeout with different TIMEOUT
  setTimeout(onthirdTimeout, TIMEOUT + 1);
  as = hooks.activitiesOfTypes('TIMERWRAP');
  assert.strictEqual(as.length, 2);
  t2 = as[1];
  assert.strictEqual(t2.type, 'TIMERWRAP');
  assert.strictEqual(typeof t2.uid, 'number');
  assert.strictEqual(typeof t2.triggerAsyncId, 'number');
  checkInvocations(t1, { init: 1, before: 2, after: 1 },
                   't1: when third timer installed');
  checkInvocations(t2, { init: 1 },
                   't2: when third timer installed');
}

function onthirdTimeout() {
  checkInvocations(t1, { init: 1, before: 2, after: 2, destroy: 1 },
                   't1: when third timer fired');
  checkInvocations(t2, { init: 1, before: 1 },
                   't2: when third timer fired');
  tick(2);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('TIMERWRAP');

  checkInvocations(t1, { init: 1, before: 2, after: 2, destroy: 1 },
                   't1: when process exits');
  checkInvocations(t2, { init: 1, before: 1, after: 1, destroy: 1 },
                   't2: when process exits');
}
