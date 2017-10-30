'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const TIMEOUT = 1;

const hooks = initHooks();
hooks.enable();

let count = 0;
const iv = setInterval(common.mustCall(oninterval, 3), TIMEOUT);

const as = hooks.activitiesOfTypes('TIMERWRAP');
assert.strictEqual(as.length, 1);
const t = as[0];
assert.strictEqual(t.type, 'TIMERWRAP');
assert.strictEqual(typeof t.uid, 'number');
assert.strictEqual(typeof t.triggerAsyncId, 'number');
checkInvocations(t, { init: 1 }, 't: when first timer installed');

function oninterval() {
  count++;
  assert.strictEqual(as.length, 1);
  switch (count) {
    case 1: {
      checkInvocations(t, { init: 1, before: 1 },
                       't: when first timer triggered first time');
      break;
    }
    case 2: {
      checkInvocations(t, { init: 1, before: 2, after: 1 },
                       't: when first timer triggered second time');
      break;
    }
    case 3: {
      clearInterval(iv);
      checkInvocations(t, { init: 1, before: 3, after: 2 },
                       't: when first timer triggered third time');
      tick(2);
      break;
    }
  }
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('TIMERWRAP');

  checkInvocations(t, { init: 1, before: 3, after: 3, destroy: 1 },
                   't: when process exits');
}
