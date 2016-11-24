'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const cp = require('child_process');
const childPath = path.join(common.fixturesDir, 'parent-process-exec-nonpersistent.js');

let persistentPid = -1;

const child = cp.fork(childPath);

child.on('message', common.mustCall((msg) => {
  persistentPid = msg;
}));

process.on('exit', () => {
  assert.notStrictEqual(persistentPid, -1);
});

setTimeout(function() {
  // *nix: test that the child process it has the gid === pid
  process.kill(persistentPid);
}, 150);