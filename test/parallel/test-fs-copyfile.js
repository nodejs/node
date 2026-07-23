// Flags: --expose-internals
'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { internalBinding } = require('internal/test/binding');

const {
  UV_ENOENT,
  UV_EEXIST
} = internalBinding('uv');

const src = fixtures.path('a.js');
const dest = tmpdir.resolve('copyfile.out');

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

// Verify overwrite behavior.
fs.writeFileSync(dest, '', 'utf8');
const result = fs.copyFileSync(src, dest);
assert.strictEqual(result, undefined);
verify(src, dest);

// Verify overwrite with default flags.
fs.copyFileSync(src, dest, 0);
verify(src, dest);

// Verify UV_FS_COPYFILE_FICLONE.
fs.unlinkSync(dest);
fs.copyFileSync(src, dest, UV_FS_COPYFILE_FICLONE);
verify(src, dest);

// Verify COPYFILE_FICLONE_FORCE.
try {
  fs.unlinkSync(dest);
  fs.copyFileSync(src, dest, COPYFILE_FICLONE_FORCE);
  verify(src, dest);
} catch (err) {
  assert.strictEqual(err.syscall, 'copyfile');
  assert(
    err.code === 'ENOTSUP' ||
    err.code === 'ENOTTY' ||
    err.code === 'ENOSYS' ||
    err.code === 'EXDEV'
  );
  assert.strictEqual(err.path, src);
  assert.strictEqual(err.dest, dest);
}

// Async copy.
tmpdir.refresh();
fs.copyFile(src, dest, common.mustSucceed(() => {
  verify(src, dest);

  fs.copyFile(src, dest, COPYFILE_EXCL, common.mustCall((err) => {
    if (err.code === 'ENOENT') {
      assert.strictEqual(err.errno, UV_ENOENT);
    } else {
      assert.strictEqual(err.errno, UV_EEXIST);
    }
  }));
}));

// Argument validation.
assert.throws(() => {
  fs.copyFile(src, dest, 0, 0);
}, {
  code: 'ERR_INVALID_ARG_TYPE'
});

[false, 1, {}, [], null, undefined].forEach((i) => {
  assert.throws(() => fs.copyFile(i, dest, () => {}), /src/);
  assert.throws(() => fs.copyFile(src, i, () => {}), /dest/);
  assert.throws(() => fs.copyFileSync(i, dest), /src/);
  assert.throws(() => fs.copyFileSync(src, i), /dest/);
});

assert.throws(() => {
  fs.copyFileSync(src, dest, 'r');
}, {
  code: 'ERR_INVALID_ARG_TYPE'
});

assert.throws(() => {
  fs.copyFileSync(src, dest, 8);
}, {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => {
  fs.copyFile(src, dest, 'r', () => {});
}, {
  code: 'ERR_INVALID_ARG_TYPE'
});

/* -------------------------------------------------
 * Symlink dereference behavior (NEW TEST)
 * ------------------------------------------------- */

tmpdir.refresh();

const target = path.join(tmpdir.path, 'target.txt');
const link = path.join(tmpdir.path, 'link.txt');
const copy = path.join(tmpdir.path, 'copy.txt');

fs.writeFileSync(target, 'hello');
fs.symlinkSync(target, link);

// copyFile() should dereference the symlink
fs.copyFileSync(link, copy);

assert.strictEqual(fs.readFileSync(copy, 'utf8'), 'hello');
assert.strictEqual(fs.lstatSync(copy).isSymbolicLink(), false);
