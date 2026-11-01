'use strict';

// This tests that fs.opendir() and fs.promises.opendir() report an invalid
// bufferSize to the caller instead of throwing from an internal callback.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

for (const bufferSize of [-1, 0, 0.5, 1.5, Infinity, NaN]) {
  const expected = { code: 'ERR_OUT_OF_RANGE', name: 'RangeError' };
  assert.throws(
    () => fs.opendir(tmpdir.path, { bufferSize }, common.mustNotCall()),
    expected,
  );
  assert.rejects(fs.promises.opendir(tmpdir.path, { bufferSize }), expected)
    .then(common.mustCall());
}

for (const bufferSize of ['', '1', null, undefined]) {
  const expected = { code: 'ERR_INVALID_ARG_TYPE', name: 'TypeError' };
  assert.throws(
    () => fs.opendir(tmpdir.path, { bufferSize }, common.mustNotCall()),
    expected,
  );
  assert.rejects(fs.promises.opendir(tmpdir.path, { bufferSize }), expected)
    .then(common.mustCall());
}

// A valid bufferSize still opens the directory.
fs.opendir(tmpdir.path, { bufferSize: 1 }, common.mustSucceed((dir) => {
  dir.closeSync();
}));
