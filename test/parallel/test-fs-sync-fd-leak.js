'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

// ensure that (read|write|append)FileSync() closes the file descriptor
fs.openSync = function() {
  return 42;
};
fs.closeSync = function(fd) {
  assert.equal(fd, 42);
  close_called++;
};
fs.readSync = function() {
  throw new Error('BAM');
};
fs.writeSync = function() {
  throw new Error('BAM');
};

fs.fstatSync = function() {
  throw new Error('BAM');
};

ensureThrows(function() {
  fs.readFileSync('dummy');
});
ensureThrows(function() {
  fs.writeFileSync('dummy', 'xxx');
});
ensureThrows(function() {
  fs.appendFileSync('dummy', 'xxx');
});

var close_called = 0;
function ensureThrows(cb) {
  var got_exception = false;

  close_called = 0;
  try {
    cb();
  } catch (e) {
    assert.equal(e.message, 'BAM');
    got_exception = true;
  }

  assert.equal(close_called, 1);
  assert.equal(got_exception, true);
}
