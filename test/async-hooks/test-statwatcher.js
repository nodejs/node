'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const fs = require('fs');
const path = require('path');

if (!common.isMainThread)
  common.skip('Worker bootstrapping works differently -> different async IDs');

tmpdir.refresh();

const file1 = path.join(tmpdir.path, 'file1');
const file2 = path.join(tmpdir.path, 'file2');
fs.writeFileSync(file1, 'foo');
fs.writeFileSync(file2, 'bar');

const hooks = initHooks();
hooks.enable();

function onchange() {}
// install first file watcher
const w1 = fs.watchFile(file1, { interval: 10 }, onchange);

let as = hooks.activitiesOfTypes('STATWATCHER');
assert.strictEqual(as.length, 1);

const statwatcher1 = as[0];
assert.strictEqual(statwatcher1.type, 'STATWATCHER');
assert.strictEqual(typeof statwatcher1.uid, 'number');
assert.strictEqual(statwatcher1.triggerAsyncId, 1);
checkInvocations(statwatcher1, { init: 1 },
                 'watcher1: when started to watch file');

// install second file watcher
const w2 = fs.watchFile(file2, { interval: 10 }, onchange);
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

setTimeout(() => fs.writeFileSync(file1, 'foo++'),
           common.platformTimeout(100));
w1.once('change', common.mustCall(() => {
  setImmediate(() => {
    checkInvocations(statwatcher1, { init: 1, before: 1, after: 1 },
                     'watcher1: when unwatched first file');
    checkInvocations(statwatcher2, { init: 1 },
                     'watcher2: when unwatched first file');

    setTimeout(() => fs.writeFileSync(file2, 'bar++'),
               common.platformTimeout(100));
    w2.once('change', common.mustCall(() => {
      setImmediate(() => {
        checkInvocations(statwatcher1, { init: 1, before: 1, after: 1 },
                         'watcher1: when unwatched second file');
        checkInvocations(statwatcher2, { init: 1, before: 1, after: 1 },
                         'watcher2: when unwatched second file');
        fs.unwatchFile(file1);
        fs.unwatchFile(file2);
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
