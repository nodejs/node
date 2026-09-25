'use strict';

const common = require('../common');
const assert = require('assert');
const tick = require('../common/tick');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const fs = require('fs');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('Worker bootstrapping works differently -> different async IDs');
}

const hooks = initHooks();

hooks.enable();
fs.readFile(__filename, common.mustCall(onread));

function onread() {
  // fs.readFile() of a small file is a single request (open + fstat + read +
  // close in one thread pool round trip); larger files continue with one
  // request per chunk. Either way each request is an FSREQCALLBACK triggered
  // by the previous one (or by the top-level for the first).
  const as = hooks.activitiesOfTypes('FSREQCALLBACK');
  assert.ok(as.length >= 1);
  let lastParent = 1;
  for (let i = 0; i < as.length; i++) {
    const a = as[i];
    assert.strictEqual(a.type, 'FSREQCALLBACK');
    assert.strictEqual(typeof a.uid, 'number');
    assert.strictEqual(a.triggerAsyncId, lastParent);
    lastParent = a.uid;
  }
  for (let i = 0; i < as.length - 1; i++) {
    checkInvocations(as[i], { init: 1, before: 1, after: 1, destroy: 1 },
                     `reqwrap[${i}]: while in onread callback`);
  }

  // This callback is called from within the last fs req callback therefore
  // the last req is still going and after/destroy haven't been called yet
  checkInvocations(as[as.length - 1], { init: 1, before: 1 },
                   `reqwrap[${as.length - 1}]: while in onread callback`);
  tick(2);
}

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('FSREQCALLBACK');
  const as = hooks.activitiesOfTypes('FSREQCALLBACK');
  const a = as.pop();
  checkInvocations(a, { init: 1, before: 1, after: 1, destroy: 1 },
                   'when process exits');
}
