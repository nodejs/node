'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const src = fixtures.path('a.js');
const dest = path.join(tmpdir.path, 'copyfile.out');
const {
  COPYFILE_EXCL,
  COPYFILE_FICLONE,
  COPYFILE_FICLONE_FORCE,
  UV_FS_COPYFILE_EXCL,
  UV_FS_COPYFILE_FICLONE,
  UV_FS_COPYFILE_FICLONE_FORCE
} = fs.constants;

function verify(src, dest) {
  const srcData = fs.readFileSync(src, 'utf8');
  const srcStat = fs.statSync(src);
  const destData = fs.readFileSync(dest, 'utf8');
  const destStat = fs.statSync(dest);

  assert.strictEqual(srcData, destData);
  assert.strictEqual(srcStat.mode, destStat.mode);
  assert.strictEqual(srcStat.size, destStat.size);
}

tmpdir.refresh();

// Verify that flags are defined.
assert.strictEqual(typeof COPYFILE_EXCL, 'number');
assert.strictEqual(typeof COPYFILE_FICLONE, 'number');
assert.strictEqual(typeof COPYFILE_FICLONE_FORCE, 'number');
assert.strictEqual(typeof UV_FS_COPYFILE_EXCL, 'number');
assert.strictEqual(typeof UV_FS_COPYFILE_FICLONE, 'number');
assert.strictEqual(typeof UV_FS_COPYFILE_FICLONE_FORCE, 'number');
assert.strictEqual(COPYFILE_EXCL, UV_FS_COPYFILE_EXCL);
assert.strictEqual(COPYFILE_FICLONE, UV_FS_COPYFILE_FICLONE);
assert.strictEqual(COPYFILE_FICLONE_FORCE, UV_FS_COPYFILE_FICLONE_FORCE);

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

// Verify that UV_FS_COPYFILE_FICLONE can be used.
fs.unlinkSync(dest);
fs.copyFileSync(src, dest, UV_FS_COPYFILE_FICLONE);
verify(src, dest);

// Verify that COPYFILE_FICLONE_FORCE can be used.
try {
  fs.unlinkSync(dest);
  fs.copyFileSync(src, dest, COPYFILE_FICLONE_FORCE);
  verify(src, dest);
} catch (err) {
  assert.strictEqual(err.syscall, 'copyfile');
  assert(err.code === 'ENOTSUP' || err.code === 'ENOTTY' ||
    err.code === 'ENOSYS');
  assert.strictEqual(err.path, src);
  assert.strictEqual(err.dest, dest);
}

// Copies asynchronously.
tmpdir.refresh(); // Don't use unlinkSync() since the last test may fail.
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
assert.throws(() => {
  fs.copyFileSync(null, dest);
}, /^TypeError: src must be a string$/);

// Throws if the source path is an invalid path.
common.expectsError(() => {
  fs.copyFileSync('\u0000', dest);
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: Error,
  message: 'The "path" argument must be of type string without null bytes.' +
           ' Received type string'
});

// Throws if the destination path is not a string.
assert.throws(() => {
  fs.copyFileSync(src, null);
}, /^TypeError: dest must be a string$/);

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
