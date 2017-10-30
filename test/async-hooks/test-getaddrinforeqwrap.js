'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('./tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const dns = require('dns');

const hooks = initHooks();

hooks.enable();
dns.lookup('www.google.com', 4, common.mustCall(onlookup));
function onlookup(err_, ip, family) {
  // we don't care about the error here in order to allow
  // tests to run offline (lookup will fail in that case and the err be set);

  const as = hooks.activitiesOfTypes('GETADDRINFOREQWRAP');
  assert.strictEqual(as.length, 1);

  const a = as[0];
  assert.strictEqual(a.type, 'GETADDRINFOREQWRAP');
  assert.strictEqual(typeof a.uid, 'number');
  assert.strictEqual(a.triggerAsyncId, 1);
  checkInvocations(a, { init: 1, before: 1 }, 'while in onlookup callback');
  tick(2);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('GETADDRINFOREQWRAP');

  const as = hooks.activitiesOfTypes('GETADDRINFOREQWRAP');
  const a = as[0];
  checkInvocations(a, { init: 1, before: 1, after: 1, destroy: 1 },
                   'when process exits');
}
