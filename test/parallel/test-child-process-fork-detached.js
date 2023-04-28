'use strict';
require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const fixtures = require('../common/fixtures');

const nonPersistentNode = fork(
  fixtures.path('parent-process-nonpersistent-fork.js'),
  [],
  { silent: true });

let childId = -1;

nonPersistentNode.stdout.on('data', (data) => {
  childId = parseInt(data, 10);
  nonPersistentNode.kill();
});

process.on('exit', () => {
  assert.notStrictEqual(childId, -1);
  // Killing the child process should not throw an error
  process.kill(childId);
});
