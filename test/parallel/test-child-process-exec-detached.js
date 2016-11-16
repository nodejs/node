'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const cp = require('child_process');
const childPath = path.join(common.fixturesDir, 'parent-process-exec-nonpersistent.js');

let persistentPid = -1;

const execParent = cp.fork(childPath);

execParent.on('message', common.mustCall((msg) => {
  persistentPid = msg;
}));

execParent.on('close', common.mustCall((code, sig) => {
  assert.equal(sig, null);
}));

process.on('exit', () => {
  assert.notStrictEqual(persistentPid, -1);
});
