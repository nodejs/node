'use strict';

const common = require('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const p = (new Promise(common.mustCall(executor)));

// init hooks after promise was created
const hooks = initHooks();
hooks._allowNoInit = true;
hooks.enable();

p.then(afterresolution);

function executor(resolve, reject) {
  resolve(5);
}

function afterresolution(val) {
  assert.strictEqual(val, 5);
  const as = hooks.activitiesOfTypes('PROMISE');
  assert.strictEqual(as.length, 1, 'one activity');
  checkInvocations(as[0], { init: 1, before: 1 },
                   'after resolution child promise');
  return val;
}

process.on('exit', onexit);
function onexit() {
  hooks.disable();
  hooks.sanityCheck('PROMISE');

  const as = hooks.activitiesOfTypes('PROMISE');
  assert.strictEqual(as.length, 1,
                     'should only get one activity(child promise)');

  const a0 = as[0];
  assert.strictEqual(a0.type, 'PROMISE', 'promise request');
  assert.strictEqual(typeof a0.uid, 'number', 'uid is a number');
  // we can't get the triggerId dynamically, we have to use static number here
  assert.strictEqual(a0.triggerId - (a0.uid - 3), 2,
                     'child promise should use parent uid as triggerId');
  // We expect a destroy hook as well but we cannot guarentee predictable gc.
  checkInvocations(a0, { init: 1, before: 1, after: 1 }, 'when process exits');
}
