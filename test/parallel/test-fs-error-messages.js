// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const nonexistentFile = fixtures.path('non-existent');
const existingFile = fixtures.path('exit.js');
const existingFile2 = fixtures.path('create-file.js');
const existingDir = fixtures.path('empty');
const existingDir2 = fixtures.path('keys');
const uv = process.binding('uv');

// Template tag function for escaping special characters in strings so that:
// new RegExp(re`${str}`).test(str) === true
function re(literals, ...values) {
  const escapeRE = /[\\^$.*+?()[\]{}|=!<>:-]/g;
  let result = literals[0].replace(escapeRE, '\\$&');
  for (const [i, value] of values.entries()) {
    result += value.replace(escapeRE, '\\$&');
    result += literals[i + 1].replace(escapeRE, '\\$&');
  }
  return result;
}

// stat
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, stat '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'stat');
    return true;
  };

  fs.stat(nonexistentFile, common.mustCall(validateError));

  assert.throws(
    () => fs.statSync(nonexistentFile),
    validateError
  );
}

// lstat
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, lstat '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'lstat');
    return true;
  };

  fs.lstat(nonexistentFile, common.mustCall(validateError));
  assert.throws(
    () => fs.lstatSync(nonexistentFile),
    validateError
  );
}

// fstat
{
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, fstat');
    assert.strictEqual(err.errno, uv.UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'fstat');
    return true;
  };

  const fd = fs.openSync(existingFile, 'r');
  fs.closeSync(fd);

  fs.fstat(fd, common.mustCall(validateError));

  assert.throws(
    () => fs.fstatSync(fd),
    validateError
  );
}

// realpath
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, lstat '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'lstat');
    return true;
  };

  fs.realpath(nonexistentFile, common.mustCall(validateError));

  assert.throws(
    () => fs.realpathSync(nonexistentFile),
    validateError
  );
}

// readlink
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, readlink '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'readlink');
    return true;
  };

  fs.readlink(nonexistentFile, common.mustCall(validateError));

  assert.throws(
    () => fs.readlinkSync(nonexistentFile),
    validateError
  );
}

// link nonexistent file
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    // Could be resolved to an absolute path
    assert.ok(err.dest.endsWith('foo'),
              `expect ${err.dest} to end with 'foo'`);
    const regexp = new RegExp('^ENOENT: no such file or directory, link ' +
                              re`'${nonexistentFile}' -> ` + '\'.*foo\'');
    assert.ok(regexp.test(err.message),
              `Expect ${err.message} to match ${regexp}`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'link');
    return true;
  };

  fs.link(nonexistentFile, 'foo', common.mustCall(validateError));

  assert.throws(
    () => fs.linkSync(nonexistentFile, 'foo'),
    validateError
  );
}

// link existing file
{
  const validateError = (err) => {
    assert.strictEqual(existingFile, err.path);
    assert.strictEqual(existingFile2, err.dest);
    assert.strictEqual(
      err.message,
      `EEXIST: file already exists, link '${existingFile}' -> ` +
      `'${existingFile2}'`);
    assert.strictEqual(err.errno, uv.UV_EEXIST);
    assert.strictEqual(err.code, 'EEXIST');
    assert.strictEqual(err.syscall, 'link');
    return true;
  };

  fs.link(existingFile, existingFile2, common.mustCall(validateError));

  assert.throws(
    () => fs.linkSync(existingFile, existingFile2),
    validateError
  );
}

// symlink
{
  const validateError = (err) => {
    assert.strictEqual(existingFile, err.path);
    assert.strictEqual(existingFile2, err.dest);
    assert.strictEqual(
      err.message,
      `EEXIST: file already exists, symlink '${existingFile}' -> ` +
      `'${existingFile2}'`);
    assert.strictEqual(err.errno, uv.UV_EEXIST);
    assert.strictEqual(err.code, 'EEXIST');
    assert.strictEqual(err.syscall, 'symlink');
    return true;
  };

  fs.symlink(existingFile, existingFile2, common.mustCall(validateError));

  assert.throws(
    () => fs.symlinkSync(existingFile, existingFile2),
    validateError
  );
}

// unlink
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, unlink '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'unlink');
    return true;
  };

  fs.unlink(nonexistentFile, common.mustCall(validateError));

  assert.throws(
    () => fs.unlinkSync(nonexistentFile),
    validateError
  );
}

// rename
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    // Could be resolved to an absolute path
    assert.ok(err.dest.endsWith('foo'),
              `expect ${err.dest} to end with 'foo'`);
    const regexp = new RegExp('ENOENT: no such file or directory, rename ' +
                              re`'${nonexistentFile}' -> ` + '\'.*foo\'');
    assert.ok(regexp.test(err.message),
              `Expect ${err.message} to match ${regexp}`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'rename');
    return true;
  };

  fs.rename(nonexistentFile, 'foo', common.mustCall(validateError));

  assert.throws(
    () => fs.renameSync(nonexistentFile, 'foo'),
    validateError
  );
}

// rename non-empty directory
{
  const validateError = (err) => {
    assert.strictEqual(existingDir, err.path);
    assert.strictEqual(existingDir2, err.dest);
    assert.strictEqual(err.syscall, 'rename');
    // Could be ENOTEMPTY, EEXIST, or EPERM, depending on the platform
    if (err.code === 'ENOTEMPTY') {
      assert.strictEqual(
        err.message,
        `ENOTEMPTY: directory not empty, rename '${existingDir}' -> ` +
        `'${existingDir2}'`);
      assert.strictEqual(err.errno, uv.UV_ENOTEMPTY);
    } else if (err.code === 'EEXIST') {  // smartos and aix
      assert.strictEqual(
        err.message,
        `EEXIST: file already exists, rename '${existingDir}' -> ` +
        `'${existingDir2}'`);
      assert.strictEqual(err.errno, uv.UV_EEXIST);
    } else {  // windows
      assert.strictEqual(
        err.message,
        `EPERM: operation not permitted, rename '${existingDir}' -> ` +
        `'${existingDir2}'`);
      assert.strictEqual(err.errno, uv.UV_EPERM);
      assert.strictEqual(err.code, 'EPERM');
    }
    return true;
  };

  fs.rename(existingDir, existingDir2, common.mustCall(validateError));

  assert.throws(
    () => fs.renameSync(existingDir, existingDir2),
    validateError
  );
}

// rmdir
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, rmdir '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'rmdir');
    return true;
  };

  fs.rmdir(nonexistentFile, common.mustCall(validateError));

  assert.throws(
    () => fs.rmdirSync(nonexistentFile),
    validateError
  );
}

// rmdir a file
{
  const validateError = (err) => {
    assert.strictEqual(existingFile, err.path);
    assert.strictEqual(err.syscall, 'rmdir');
    if (err.code === 'ENOTDIR') {
      assert.strictEqual(
        err.message,
        `ENOTDIR: not a directory, rmdir '${existingFile}'`);
      assert.strictEqual(err.errno, uv.UV_ENOTDIR);
    } else {  // windows
      assert.strictEqual(
        err.message,
        `ENOENT: no such file or directory, rmdir '${existingFile}'`);
      assert.strictEqual(err.errno, uv.UV_ENOENT);
      assert.strictEqual(err.code, 'ENOENT');
    }
    return true;
  };

  fs.rmdir(existingFile, common.mustCall(validateError));

  assert.throws(
    () => fs.rmdirSync(existingFile),
    validateError
  );
}

// mkdir
{
  const validateError = (err) => {
    assert.strictEqual(existingFile, err.path);
    assert.strictEqual(
      err.message,
      `EEXIST: file already exists, mkdir '${existingFile}'`);
    assert.strictEqual(err.errno, uv.UV_EEXIST);
    assert.strictEqual(err.code, 'EEXIST');
    assert.strictEqual(err.syscall, 'mkdir');
    return true;
  };

  fs.mkdir(existingFile, 0o666, common.mustCall(validateError));

  assert.throws(
    () => fs.mkdirSync(existingFile, 0o666),
    validateError
  );
}

// chmod
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, chmod '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'chmod');
    return true;
  };

  fs.chmod(nonexistentFile, 0o666, common.mustCall(validateError));

  assert.throws(
    () => fs.chmodSync(nonexistentFile, 0o666),
    validateError
  );
}

// open
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, open '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'open');
    return true;
  };

  fs.open(nonexistentFile, 'r', 0o666, common.mustCall(validateError));

  assert.throws(
    () => fs.openSync(nonexistentFile, 'r', 0o666),
    validateError
  );
}

// readFile
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, open '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'open');
    return true;
  };

  fs.readFile(nonexistentFile, common.mustCall(validateError));

  assert.throws(
    () => fs.readFileSync(nonexistentFile),
    validateError
  );
}

// readdir
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, scandir '${nonexistentFile}'`);
    assert.strictEqual(err.errno, uv.UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'scandir');
    return true;
  };

  fs.readdir(nonexistentFile, common.mustCall(validateError));

  assert.throws(
    () => fs.readdirSync(nonexistentFile),
    validateError
  );
}
