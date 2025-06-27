'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { promises } = fs;

// Validate the path argument.
[false, 1, {}, [], null, undefined].forEach((i) => {
  const err = { name: 'TypeError', code: 'ERR_INVALID_ARG_TYPE' };

  assert.throws(() => fs.lchown(i, 1, 1, common.mustNotCall()), err);
  assert.throws(() => fs.lchownSync(i, 1, 1), err);
  promises.lchown(false, 1, 1)
    .then(common.mustNotCall())
    .catch(common.expectsError(err));
});

// Validate the uid and gid arguments.
[false, 'test', {}, [], null, undefined].forEach((i) => {
  const err = { name: 'TypeError', code: 'ERR_INVALID_ARG_TYPE' };

  assert.throws(
    () => fs.lchown('not_a_file_that_exists', i, 1, common.mustNotCall()),
    err
  );
  assert.throws(
    () => fs.lchown('not_a_file_that_exists', 1, i, common.mustNotCall()),
    err
  );
  assert.throws(() => fs.lchownSync('not_a_file_that_exists', i, 1), err);
  assert.throws(() => fs.lchownSync('not_a_file_that_exists', 1, i), err);

  promises.lchown('not_a_file_that_exists', i, 1)
    .then(common.mustNotCall())
    .catch(common.expectsError(err));

  promises.lchown('not_a_file_that_exists', 1, i)
    .then(common.mustNotCall())
    .catch(common.expectsError(err));
});

// Validate the callback argument.
[false, 1, 'test', {}, [], null, undefined].forEach((i) => {
  assert.throws(() => fs.lchown('not_a_file_that_exists', 1, 1, i), {
    name: 'TypeError',
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

if (!common.isWindows) {
  const testFile = tmpdir.resolve(path.basename(__filename));
  const uid = process.geteuid();
  const gid = process.getegid();

  tmpdir.refresh();
  fs.copyFileSync(__filename, testFile);
  fs.lchownSync(testFile, uid, gid);
  fs.lchown(testFile, uid, gid, common.mustSucceed(async (err) => {
    await promises.lchown(testFile, uid, gid);
  }));
}
