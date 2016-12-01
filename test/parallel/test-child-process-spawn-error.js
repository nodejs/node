'use strict';
var common = require('../common');
var spawn = require('child_process').spawn;
var assert = require('assert');

var enoentPath = 'foo123';
var spawnargs = ['bar'];
assert.strictEqual(common.fileExists(enoentPath), false);

var enoentChild = spawn(enoentPath, spawnargs);
enoentChild.on('error', common.mustCall(function(err) {
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.errno, 'ENOENT');
  assert.strictEqual(err.syscall, 'spawn ' + enoentPath);
  assert.strictEqual(err.path, enoentPath);
  assert.deepStrictEqual(err.spawnargs, spawnargs);
}));
