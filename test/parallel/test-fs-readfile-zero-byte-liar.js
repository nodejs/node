'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

var dataExpected = fs.readFileSync(__filename, 'utf8');

// sometimes stat returns size=0, but it's a lie.
fs._fstat = fs.fstat;
fs._fstatSync = fs.fstatSync;

fs.fstat = function(fd, cb) {
  fs._fstat(fd, function(er, st) {
    if (er) return cb(er);
    st.size = 0;
    return cb(er, st);
  });
};

fs.fstatSync = function(fd) {
  var st = fs._fstatSync(fd);
  st.size = 0;
  return st;
};

var d = fs.readFileSync(__filename, 'utf8');
assert.equal(d, dataExpected);

var called = false;
fs.readFile(__filename, 'utf8', function(er, d) {
  assert.equal(d, dataExpected);
  called = true;
});

process.on('exit', function() {
  assert(called);
  console.log('ok');
});
