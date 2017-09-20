'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const hooks = initHooks();
hooks.enable();

// install first immediate
setImmediate(common.mustCall(onimmediate));

const as = hooks.activitiesOfTypes('Immediate');
assert.strictEqual(as.length, 1);
const imd1 = as[0];
assert.strictEqual(imd1.type, 'Immediate');
assert.strictEqual(typeof imd1.uid, 'number');
assert.strictEqual(typeof imd1.triggerAsyncId, 'number');
checkInvocations(imd1, { init: 1 },
                 'imd1: when first set immediate installed');

let imd2;

function onimmediate() {
  let as = hooks.activitiesOfTypes('Immediate');
  assert.strictEqual(as.length, 1);
  checkInvocations(imd1, { init: 1, before: 1 },
                   'imd1: when first set immediate triggered');

  // install second immediate
  setImmediate(common.mustCall(onimmediateTwo));
  as = hooks.activitiesOfTypes('Immediate');
  assert.strictEqual(as.length, 2);
  imd2 = as[1];
  assert.strictEqual(imd2.type, 'Immediate');
  assert.strictEqual(typeof imd2.uid, 'number');
  assert.strictEqual(typeof imd2.triggerAsyncId, 'number');
  checkInvocations(imd1, { init: 1, before: 1 },
                   'imd1: when second set immediate installed');
  checkInvocations(imd2, { init: 1 },
                   'imd2: when second set immediate installed');
}

function onimmediateTwo() {
  checkInvocations(imd1, { init: 1, before: 1, after: 1, destroy: 1 },
                   'imd1: when second set immediate triggered');
  checkInvocations(imd2, { init: 1, before: 1 },
                   'imd2: when second set immediate triggered');
  tick(1);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('Immediate');
  checkInvocations(imd1, { init: 1, before: 1, after: 1, destroy: 1 },
                   'imd1: when process exits');
  checkInvocations(imd2, { init: 1, before: 1, after: 1, destroy: 1 },
                   'imd2: when process exits');
}
