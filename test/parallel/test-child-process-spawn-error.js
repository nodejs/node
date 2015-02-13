var common = require('../common');
var fs = require('fs');
var spawn = require('child_process').spawn;
var assert = require('assert');

var errors = 0;

var enoentPath = 'foo123';
var spawnargs = ['bar'];
assert.equal(common.fileExists(enoentPath), false);

var enoentChild = spawn(enoentPath, spawnargs);
enoentChild.on('error', function (err) {
  assert.equal(err.code, 'ENOENT');
  assert.equal(err.errno, 'ENOENT');
  assert.equal(err.syscall, 'spawn ' + enoentPath);
  assert.equal(err.path, enoentPath);
  assert.deepEqual(err.spawnargs, spawnargs);
  errors++;
});

process.on('exit', function() {
  assert.equal(1, errors);
});
