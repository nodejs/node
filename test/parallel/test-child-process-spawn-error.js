'use strict';
const common = require('../common');
const spawn = require('child_process').spawn;
const assert = require('assert');

const enoentPath = 'foo123';
const spawnargs = ['bar'];
assert.equal(common.fileExists(enoentPath), false);

const enoentChild = spawn(enoentPath, spawnargs);
enoentChild.on('error', common.mustCall(function(err) {
  assert.equal(err.code, 'ENOENT');
  assert.equal(err.errno, 'ENOENT');
  assert.equal(err.syscall, 'spawn ' + enoentPath);
  assert.equal(err.path, enoentPath);
  assert.deepEqual(err.spawnargs, spawnargs);
}));
