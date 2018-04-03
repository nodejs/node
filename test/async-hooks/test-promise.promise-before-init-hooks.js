'use strict';

const common = require('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');

const p = new Promise(common.mustCall(function executor(resolve, reject) {
  resolve(5);
}));

// init hooks after promise was created
const hooks = initHooks({ allowNoInit: true });
hooks.enable();

p.then(function afterresolution(val) {
  assert.strictEqual(val, 5);
  const as = hooks.activitiesOfTypes('PROMISE');
  assert.strictEqual(as.length, 1);
  checkInvocations(as[0], { init: 1, before: 1 },
                   'after resolution child promise');
  return val;
});

process.on('exit', function onexit() {
  hooks.disable();
  hooks.sanityCheck('PROMISE');

  const as = hooks.activitiesOfTypes('PROMISE');
  assert.strictEqual(as.length, 1);

  const a0 = as[0];
  assert.strictEqual(a0.type, 'PROMISE');
  assert.strictEqual(typeof a0.uid, 'number');
  // we can't get the asyncId from the parent dynamically, since init was
  // never called. However, it is known that the parent promise was created
  // immediately before the child promise, thus there should only be one
  // difference in id.
  assert.strictEqual(a0.triggerAsyncId, a0.uid - 1);
  // We expect a destroy hook as well but we cannot guarantee predictable gc.
  checkInvocations(a0, { init: 1, before: 1, after: 1 }, 'when process exits');
});
