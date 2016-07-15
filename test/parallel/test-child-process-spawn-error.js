'use strict';
var common = require('../common');
var spawn = require('child_process').spawn;
var assert = require('assert');

var enoentPath = 'foo123';
var spawnargs = ['bar'];
assert.equal(common.fileExists(enoentPath), false);

var enoentChild = spawn(enoentPath, spawnargs);
enoentChild.on('error', common.mustCall(function(err) {
  assert.equal(err.code, 'ENOENT');
  assert.equal(err.errno, 'ENOENT');
  assert.equal(err.syscall, 'spawn ' + enoentPath);
  assert.equal(err.path, enoentPath);
  assert.deepStrictEqual(err.spawnargs, spawnargs);
}));
