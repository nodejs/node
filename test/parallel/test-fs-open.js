'use strict';
var constants = require('constants');
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

var caughtException = false;
try {
  // should throw ENOENT, not EBADF
  // see https://github.com/joyent/node/pull/1228
  fs.openSync('/path/to/file/that/does/not/exist', 'r');
}
catch (e) {
  assert.equal(e.code, 'ENOENT');
  caughtException = true;
}
assert.ok(caughtException);

var openFd;
fs.open(__filename, 'r', function(err, fd) {
  if (err) {
    throw err;
  }
  openFd = fd;
});

var openFd2;
fs.open(__filename, 'rs', function(err, fd) {
  if (err) {
    throw err;
  }
  openFd2 = fd;
});

process.on('exit', function() {
  assert.ok(openFd);
  assert.ok(openFd2);
});

