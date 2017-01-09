'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const dataExpected = fs.readFileSync(__filename, 'utf8');

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
  const st = fs._fstatSync(fd);
  st.size = 0;
  return st;
};

const d = fs.readFileSync(__filename, 'utf8');
assert.strictEqual(d, dataExpected);

fs.readFile(__filename, 'utf8', common.mustCall(function(er, d) {
  assert.strictEqual(d, dataExpected);
}));
