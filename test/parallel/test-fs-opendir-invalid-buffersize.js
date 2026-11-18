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

// bufferSize is read only once, so a getter can't pass the validation and then
// return an invalid value when the directory has been opened.
{
  let reads = 0;
  const options = {
    get bufferSize() {
      return reads++ === 0 ? 1 : 0;
    },
  };
  fs.opendir(tmpdir.path, options, common.mustSucceed((dir) => {
    assert.strictEqual(reads, 1);
    dir.closeSync();
  }));
}

// The other options are copied and validated before the directory is opened,
// so a getter can't pass the checks and then hand an invalid value to the Dir
// constructor. The call either fails or succeeds, never from the internal
// callback.
for (let validReads = 0; validReads < 5; validReads++) {
  let reads = 0;
  const options = {
    get encoding() {
      return reads++ < validReads ? 'utf8' : 'bogus';
    },
  };
  fs.promises.opendir(tmpdir.path, options).then(
    (dir) => dir.closeSync(),
    (err) => {
      if (err.code !== 'ERR_INVALID_ARG_VALUE') throw err;
    },
  ).then(common.mustCall());
}

// A non-enumerable bufferSize is ignored, the same as in fs.opendirSync().
{
  const options = {};
  Object.defineProperty(options, 'bufferSize', { value: 0 });
  fs.opendirSync(tmpdir.path, options).closeSync();
  fs.opendir(tmpdir.path, options, common.mustSucceed((dir) => {
    dir.closeSync();
  }));
}
