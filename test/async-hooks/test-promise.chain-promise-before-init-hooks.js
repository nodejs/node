'use strict';

const common = require('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const p = (new Promise(common.mustCall(executor)));
p.then(afterresolution);

// init hooks after chained promise is created
const hooks = initHooks();
hooks._allowNoInit = true;
hooks.enable();

function executor(resolve, reject) {
  resolve(5);
}

function afterresolution(val) {
  assert.strictEqual(val, 5);
  return val;
}

process.on('exit', onexit);
function onexit() {
  hooks.disable();
  hooks.sanityCheck('PROMISE');

  const as = hooks.activitiesOfTypes('PROMISE');
  const unknown = hooks.activitiesOfTypes('Unknown');
  assert.strictEqual(as.length, 0, 'no activities with PROMISE type');
  assert.strictEqual(unknown.length, 1,
                     'one activity with Unknown type which in fact is PROMISE');

  const a0 = unknown[0];
  assert.strictEqual(a0.type, 'Unknown',
                     'unknown request which in fact is PROMISE');
  assert.strictEqual(typeof a0.uid, 'number', 'uid is a number');
  checkInvocations(a0, { before: 1, after: 1 }, 'when process exits');
}
