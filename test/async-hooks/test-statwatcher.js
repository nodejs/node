'use strict';

require('../common');
const commonPath = require.resolve('../common');
const assert = require('assert');
const initHooks = require('./init-hooks');
const { checkInvocations } = require('./hook-checks');
const fs = require('fs');

const hooks = initHooks();
hooks.enable();

function onchange() {}
// install first file watcher
fs.watchFile(__filename, onchange);

let as = hooks.activitiesOfTypes('STATWATCHER');
assert.strictEqual(as.length, 1);

const statwatcher1 = as[0];
assert.strictEqual(statwatcher1.type, 'STATWATCHER');
assert.strictEqual(typeof statwatcher1.uid, 'number');
assert.strictEqual(statwatcher1.triggerAsyncId, 1);
checkInvocations(statwatcher1, { init: 1 },
                 'watcher1: when started to watch file');

// install second file watcher
fs.watchFile(commonPath, onchange);
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

// remove first file watcher
fs.unwatchFile(__filename);
checkInvocations(statwatcher1, { init: 1, before: 1, after: 1 },
                 'watcher1: when unwatched first file');
checkInvocations(statwatcher2, { init: 1 },
                 'watcher2: when unwatched first file');

// remove second file watcher
fs.unwatchFile(commonPath);
checkInvocations(statwatcher1, { init: 1, before: 1, after: 1 },
                 'watcher1: when unwatched second file');
checkInvocations(statwatcher2, { init: 1, before: 1, after: 1 },
                 'watcher2: when unwatched second file');

process.on('exit', onexit);

function onexit() {
  hooks.disable();
  hooks.sanityCheck('STATWATCHER');
  checkInvocations(statwatcher1, { init: 1, before: 1, after: 1 },
                   'watcher1: when process exits');
  checkInvocations(statwatcher2, { init: 1, before: 1, after: 1 },
                   'watcher2: when process exits');
}
