'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const src = fixtures.path('a.js');
const dest = path.join(common.tmpDir, 'copyfile.out');
const { COPYFILE_EXCL, UV_FS_COPYFILE_EXCL } = fs.constants;

function verify(src, dest) {
  const srcData = fs.readFileSync(src, 'utf8');
  const srcStat = fs.statSync(src);
  const destData = fs.readFileSync(dest, 'utf8');
  const destStat = fs.statSync(dest);

  assert.strictEqual(srcData, destData);
  assert.strictEqual(srcStat.mode, destStat.mode);
  assert.strictEqual(srcStat.size, destStat.size);
}

common.refreshTmpDir();

// Verify that flags are defined.
assert.strictEqual(typeof COPYFILE_EXCL, 'number');
assert.strictEqual(typeof UV_FS_COPYFILE_EXCL, 'number');
assert.strictEqual(COPYFILE_EXCL, UV_FS_COPYFILE_EXCL);

// Verify that files are overwritten when no flags are provided.
fs.writeFileSync(dest, '', 'utf8');
const result = fs.copyFileSync(src, dest);
assert.strictEqual(result, undefined);
verify(src, dest);

// Verify that files are overwritten with default flags.
fs.copyFileSync(src, dest, 0);
verify(src, dest);

// Throws if destination exists and the COPYFILE_EXCL flag is provided.
assert.throws(() => {
  fs.copyFileSync(src, dest, COPYFILE_EXCL);
}, /^Error: EEXIST|ENOENT:.+, copyfile/);

// Throws if the source does not exist.
assert.throws(() => {
  fs.copyFileSync(`${src}__does_not_exist`, dest, COPYFILE_EXCL);
}, /^Error: ENOENT: no such file or directory, copyfile/);

// Copies asynchronously.
fs.unlinkSync(dest);
fs.copyFile(src, dest, common.mustCall((err) => {
  assert.ifError(err);
  verify(src, dest);

  // Copy asynchronously with flags.
  fs.copyFile(src, dest, COPYFILE_EXCL, common.mustCall((err) => {
    assert(
      /^Error: EEXIST: file already exists, copyfile/.test(err.toString())
    );
  }));
}));

// Throws if callback is not a function.
common.expectsError(() => {
  fs.copyFile(src, dest, 0, 0);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
  message: 'The "callback" argument must be of type Function'
});

// Throws if the source path is not a string.
[false, 1, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.copyFile(i, dest, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.copyFile(src, i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.copyFileSync(i, dest),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.copyFileSync(src, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});

// Throws if the source path is an invalid path.
common.expectsError(() => {
  fs.copyFileSync('\u0000', dest);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: Error,
  message: 'The "path" argument must be of type string without null bytes.' +
           ' Received type string'
});

// Throws if the destination path is an invalid path.
common.expectsError(() => {
  fs.copyFileSync(src, '\u0000');
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: Error,
  message: 'The "path" argument must be of type string without null bytes.' +
           ' Received type string'
});

// Errors if invalid flags are provided.
assert.throws(() => {
  fs.copyFileSync(src, dest, -1);
}, /^Error: EINVAL: invalid argument, copyfile/);
