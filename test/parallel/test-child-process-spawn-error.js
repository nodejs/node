'use strict';
const common = require('../common');
const spawn = require('child_process').spawn;
const assert = require('assert');

const enoentPath = 'foo123';
const spawnargs = ['bar'];
assert.strictEqual(common.fileExists(enoentPath), false);

const enoentChild = spawn(enoentPath, spawnargs);
enoentChild.on('error', common.mustCall(function(err) {
  assert.strictEqual(err.code, 'ENOENT');
  assert.strictEqual(err.errno, 'ENOENT');
  assert.strictEqual(err.syscall, 'spawn ' + enoentPath);
  assert.strictEqual(err.path, enoentPath);
  assert.deepStrictEqual(err.spawnargs, spawnargs);
}));
