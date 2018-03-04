'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const uv = process.binding('uv');
const path = require('path');
const src = fixtures.path('a.js');
const dest = path.join(tmpdir.path, 'copyfile.out');
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

tmpdir.refresh();

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


// Copies asynchronously.
fs.unlinkSync(dest);
fs.copyFile(src, dest, common.mustCall((err) => {
  assert.ifError(err);
  verify(src, dest);

  // Copy asynchronously with flags.
  fs.copyFile(src, dest, COPYFILE_EXCL, common.mustCall((err) => {
    if (err.code === 'ENOENT') {  // Could be ENOENT or EEXIST
      assert.strictEqual(err.message,
                         'ENOENT: no such file or directory, copyfile ' +
                         `'${src}' -> '${dest}'`);
      assert.strictEqual(err.errno, uv.UV_ENOENT);
      assert.strictEqual(err.code, 'ENOENT');
      assert.strictEqual(err.syscall, 'copyfile');
    } else {
      assert.strictEqual(err.message,
                         'EEXIST: file already exists, copyfile ' +
                         `'${src}' -> '${dest}'`);
      assert.strictEqual(err.errno, uv.UV_EEXIST);
      assert.strictEqual(err.code, 'EEXIST');
      assert.strictEqual(err.syscall, 'copyfile');
    }
  }));
}));

// Throws if callback is not a function.
common.expectsError(() => {
  fs.copyFile(src, dest, 0, 0);
}, {
  code: 'ERR_INVALID_CALLBACK',
  type: TypeError,
});

// Throws if the source path is not a string.
[false, 1, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.copyFile(i, dest, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
    }
  );
  common.expectsError(
    () => fs.copyFile(src, i, common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
    }
  );
  common.expectsError(
    () => fs.copyFileSync(i, dest),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
    }
  );
  common.expectsError(
    () => fs.copyFileSync(src, i),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
    }
  );
});
