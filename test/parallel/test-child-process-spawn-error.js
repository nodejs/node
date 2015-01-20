var common = require('../common');
var fs = require('fs');
var spawn = require('child_process').spawn;
var assert = require('assert');

var errors = 0;

var enoentPath = 'foo123';
assert.equal(common.fileExists(enoentPath), false);

var enoentChild = spawn(enoentPath);
enoentChild.on('error', function (err) {
  assert.equal(err.path, enoentPath);
  errors++;
});

process.on('exit', function() {
  assert.equal(1, errors);
});
