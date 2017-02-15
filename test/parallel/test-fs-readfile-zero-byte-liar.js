'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const dataExpected = fs.readFileSync(__filename, 'utf8');

// sometimes stat returns size=0, but it's a lie.
fs._fstat = fs.fstat;
fs._fstatSync = fs.fstatSync;

fs.fstat = (fd, cb) => {
  fs._fstat(fd, (er, st) => {
    if (er) return cb(er);
    st.size = 0;
    return cb(er, st);
  });
};

fs.fstatSync = (fd) => {
  const st = fs._fstatSync(fd);
  st.size = 0;
  return st;
};

const d = fs.readFileSync(__filename, 'utf8');
assert.strictEqual(d, dataExpected);

fs.readFile(__filename, 'utf8', common.mustCall((er, d) => {
  assert.strictEqual(d, dataExpected);
}));
