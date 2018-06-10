'use strict';

const common = require('../common');
const commonPath = require.resolve('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const fs = require('fs');

if (!common.isMainThread)
  common.skip('Worker bootstrapping works differently -> different async IDs');

const hooks = initHooks();
hooks.enable();

function onchange() {}
// install first file watcher
const w1 = fs.watchFile(__filename, { interval: 10 }, onchange);

let as = hooks.activitiesOfTypes('STATWATCHER');
assert.strictEqual(as.length, 1);

const statwatcher1 = as[0];
assert.strictEqual(statwatcher1.type, 'STATWATCHER');
assert.strictEqual(typeof statwatcher1.uid, 'number');
assert.strictEqual(statwatcher1.triggerAsyncId, 1);
checkInvocations(statwatcher1, { init: 1 },
                 'watcher1: when started to watch file');

// install second file watcher
const w2 = fs.watchFile(commonPath, { interval: 10 }, onchange);
as = hooks.activitiesOfTypes('STATWATCHER');
assert.strictEqual(as.length, 2);

const statwatcher2 = as[1];
assert.strictEqual(statwatcher2.type, 'STATWATCHER');
assert.strictEqual(typeof statwatcher2.uid, 'number');
assert.strictEqual(statwatcher2.triggerAsyncId, 1);
checkInvocations(statwatcher1, { init: 1 },
                 'watcher1: when started to watch second file');
checkInvocations(statwatcher2, { init: 1 },
                 'watcher2: when started to watch second file');

// Touch the first file by modifying its access time.
const origStat = fs.statSync(__filename);
fs.utimesSync(__filename, Date.now() + 10, origStat.mtime);
w1.once('change', common.mustCall(() => {
  setImmediate(() => {
    checkInvocations(statwatcher1, { init: 1, before: 1, after: 1 },
                     'watcher1: when unwatched first file');
    checkInvocations(statwatcher2, { init: 1 },
                     'watcher2: when unwatched first file');

    // Touch the second file by modifying its access time.
    const origStat = fs.statSync(commonPath);
    fs.utimesSync(commonPath, Date.now() + 10, origStat.mtime);
    w2.once('change', common.mustCall(() => {
      setImmediate(() => {
        checkInvocations(statwatcher1, { init: 1, before: 1, after: 1 },
                         'watcher1: when unwatched second file');
        checkInvocations(statwatcher2, { init: 1, before: 1, after: 1 },
                         'watcher2: when unwatched second file');
        w1.stop();
        w2.stop();
      });
    }));
  });
}));

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('STATWATCHER');
  checkInvocations(statwatcher1, { init: 1, before: 1, after: 1 },
                   'watcher1: when process exits');
  checkInvocations(statwatcher2, { init: 1, before: 1, after: 1 },
                   'watcher2: when process exits');
}
