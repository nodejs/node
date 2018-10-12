'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('../common/tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const TIMEOUT = common.platformTimeout(100);

const hooks = initHooks();
hooks.enable();

// install first timeout
setTimeout(common.mustCall(ontimeout), TIMEOUT);
const as = hooks.activitiesOfTypes('Timeout');
assert.strictEqual(as.length, 1);
const t1 = as[0];
assert.strictEqual(t1.type, 'Timeout');
assert.strictEqual(typeof t1.uid, 'number');
assert.strictEqual(typeof t1.triggerAsyncId, 'number');
checkInvocations(t1, { init: 1 }, 't1: when first timer installed');

let timer;
let t2;
function ontimeout() {
  checkInvocations(t1, { init: 1, before: 1 }, 't1: when first timer fired');

  setTimeout(onSecondTimeout, TIMEOUT).unref();
  const as = hooks.activitiesOfTypes('Timeout');
  t2 = as[1];
  assert.strictEqual(as.length, 2);
  checkInvocations(t1, { init: 1, before: 1 },
                   't1: when second timer installed');
  checkInvocations(t2, { init: 1 },
                   't2: when second timer installed');

  timer = setTimeout(common.mustNotCall(), 2 ** 31 - 1);
}

function onSecondTimeout() {
  const as = hooks.activitiesOfTypes('Timeout');
  assert.strictEqual(as.length, 3);
  checkInvocations(t1, { init: 1, before: 1, after: 1 },
                   't1: when second timer fired');
  checkInvocations(t2, { init: 1, before: 1 },
                   't2: when second timer fired');
  clearTimeout(timer);
  tick(2);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('Timeout');

  checkInvocations(t1, { init: 1, before: 1, after: 1, destroy: 1 },
                   't1: when process exits');
  checkInvocations(t2, { init: 1, before: 1, after: 1, destroy: 1 },
                   't2: when process exits');
}
