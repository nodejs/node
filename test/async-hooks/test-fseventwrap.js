'use strict';
require('../common');

const assert = require('assert');
const initHooks = require('./init-hooks');
const tick = require('./tick');
const { checkInvocations } = require('./hook-checks');
const fs = require('fs');

const hooks = initHooks();

hooks.enable();
const watcher = fs.watch(__filename, onwatcherChanged);
function onwatcherChanged() { }

watcher.close();
tick(2);

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('FSEVENTWRAP');

  const as = hooks.activitiesOfTypes('FSEVENTWRAP');
  assert.strictEqual(as.length, 1);

  const a = as[0];
  assert.strictEqual(a.type, 'FSEVENTWRAP');
  assert.strictEqual(typeof a.uid, 'number');
  assert.strictEqual(a.triggerAsyncId, 1);
  checkInvocations(a, { init: 1, destroy: 1 }, 'when process exits');
}
