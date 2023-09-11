// Flags: --expose-internals
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
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');

tmpdir.refresh();


const nonexistentFile = tmpdir.resolve('non-existent');
const nonexistentDir = tmpdir.resolve('non-existent', 'foo', 'bar');
const existingFile = tmpdir.resolve('existingFile.js');
const existingFile2 = tmpdir.resolve('existingFile2.js');
const existingDir = tmpdir.resolve('dir');
const existingDir2 = fixtures.path('keys');
fs.mkdirSync(existingDir);
fs.writeFileSync(existingFile, 'test', 'utf-8');
fs.writeFileSync(existingFile2, 'test', 'utf-8');


const { COPYFILE_EXCL } = fs.constants;
const { internalBinding } = require('internal/test/binding');
const {
  UV_EBADF,
  UV_EEXIST,
  UV_EINVAL,
  UV_ENOENT,
  UV_ENOTDIR,
  UV_ENOTEMPTY,
  UV_EPERM
} = internalBinding('uv');

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
    assert.strictEqual(err.errno, UV_ENOENT);
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
    assert.strictEqual(err.errno, UV_ENOENT);
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
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'fstat');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.fstat(fd, common.mustCall(validateError));

    assert.throws(
      () => fs.fstatSync(fd),
      validateError
    );
  });
}

// realpath
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, lstat '${nonexistentFile}'`);
    assert.strictEqual(err.errno, UV_ENOENT);
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

// native realpath
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, realpath '${nonexistentFile}'`);
    assert.strictEqual(err.errno, UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'realpath');
    return true;
  };

  fs.realpath.native(nonexistentFile, common.mustCall(validateError));

  assert.throws(
    () => fs.realpathSync.native(nonexistentFile),
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
    assert.strictEqual(err.errno, UV_ENOENT);
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

// Link nonexistent file
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    // Could be resolved to an absolute path
    assert.ok(err.dest.endsWith('foo'),
              `expect ${err.dest} to end with 'foo'`);
    const regexp = new RegExp('^ENOENT: no such file or directory, link ' +
                              re`'${nonexistentFile}' -> ` + '\'.*foo\'');
    assert.match(err.message, regexp);
    assert.strictEqual(err.errno, UV_ENOENT);
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
    assert.strictEqual(err.errno, UV_EEXIST);
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
    assert.strictEqual(err.errno, UV_EEXIST);
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
    assert.strictEqual(err.errno, UV_ENOENT);
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
    assert.match(err.message, regexp);
    assert.strictEqual(err.errno, UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'rename');
    return true;
  };

  const destFile = tmpdir.resolve('foo');
  fs.rename(nonexistentFile, destFile, common.mustCall(validateError));

  assert.throws(
    () => fs.renameSync(nonexistentFile, destFile),
    validateError
  );
}

// Rename non-empty directory
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
      assert.strictEqual(err.errno, UV_ENOTEMPTY);
    } else if (err.code === 'EXDEV') {  // Not on the same mounted filesystem
      assert.strictEqual(
        err.message,
        `EXDEV: cross-device link not permitted, rename '${existingDir}' -> ` +
            `'${existingDir2}'`);
    } else if (err.code === 'EEXIST') {  // smartos and aix
      assert.strictEqual(
        err.message,
        `EEXIST: file already exists, rename '${existingDir}' -> ` +
        `'${existingDir2}'`);
      assert.strictEqual(err.errno, UV_EEXIST);
    } else {  // windows
      assert.strictEqual(
        err.message,
        `EPERM: operation not permitted, rename '${existingDir}' -> ` +
        `'${existingDir2}'`);
      assert.strictEqual(err.errno, UV_EPERM);
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
    assert.strictEqual(err.errno, UV_ENOENT);
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
      assert.strictEqual(err.errno, UV_ENOTDIR);
    } else {  // windows
      assert.strictEqual(
        err.message,
        `ENOENT: no such file or directory, rmdir '${existingFile}'`);
      assert.strictEqual(err.errno, UV_ENOENT);
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
    assert.strictEqual(err.errno, UV_EEXIST);
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
    assert.strictEqual(err.errno, UV_ENOENT);
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
    assert.strictEqual(err.errno, UV_ENOENT);
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


// close
{
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, close');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'close');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.close(fd, common.mustCall(validateError));

    assert.throws(
      () => fs.closeSync(fd),
      validateError
    );
  });
}

// readFile
{
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, open '${nonexistentFile}'`);
    assert.strictEqual(err.errno, UV_ENOENT);
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
    assert.strictEqual(err.errno, UV_ENOENT);
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

// ftruncate
{
  const validateError = (err) => {
    assert.strictEqual(err.syscall, 'ftruncate');
    // Could be EBADF or EINVAL, depending on the platform
    if (err.code === 'EBADF') {
      assert.strictEqual(err.message, 'EBADF: bad file descriptor, ftruncate');
      assert.strictEqual(err.errno, UV_EBADF);
    } else {
      assert.strictEqual(err.message, 'EINVAL: invalid argument, ftruncate');
      assert.strictEqual(err.errno, UV_EINVAL);
      assert.strictEqual(err.code, 'EINVAL');
    }
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.ftruncate(fd, 4, common.mustCall(validateError));

    assert.throws(
      () => fs.ftruncateSync(fd, 4),
      validateError
    );
  });
}

// fdatasync
{
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, fdatasync');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'fdatasync');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.fdatasync(fd, common.mustCall(validateError));

    assert.throws(
      () => fs.fdatasyncSync(fd),
      validateError
    );
  });
}

// fsync
{
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, fsync');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'fsync');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.fsync(fd, common.mustCall(validateError));

    assert.throws(
      () => fs.fsyncSync(fd),
      validateError
    );
  });
}

// chown
if (!common.isWindows) {
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, chown '${nonexistentFile}'`);
    assert.strictEqual(err.errno, UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'chown');
    return true;
  };

  fs.chown(nonexistentFile, process.getuid(), process.getgid(),
           common.mustCall(validateError));

  assert.throws(
    () => fs.chownSync(nonexistentFile,
                       process.getuid(), process.getgid()),
    validateError
  );
}

// utimes
if (!common.isAIX) {
  const validateError = (err) => {
    assert.strictEqual(nonexistentFile, err.path);
    assert.strictEqual(
      err.message,
      `ENOENT: no such file or directory, utime '${nonexistentFile}'`);
    assert.strictEqual(err.errno, UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'utime');
    return true;
  };

  fs.utimes(nonexistentFile, new Date(), new Date(),
            common.mustCall(validateError));

  assert.throws(
    () => fs.utimesSync(nonexistentFile, new Date(), new Date()),
    validateError
  );
}

// mkdtemp
{
  const validateError = (err) => {
    const pathPrefix = new RegExp('^' + re`${nonexistentDir}`);
    assert.match(err.path, pathPrefix);

    const prefix = new RegExp('^ENOENT: no such file or directory, mkdtemp ' +
                              re`'${nonexistentDir}`);
    assert.match(err.message, prefix);

    assert.strictEqual(err.errno, UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'mkdtemp');
    return true;
  };

  fs.mkdtemp(nonexistentDir, common.mustCall(validateError));

  assert.throws(
    () => fs.mkdtempSync(nonexistentDir),
    validateError
  );
}

// Check copyFile with invalid modes.
{
  const validateError = {
    code: 'ERR_OUT_OF_RANGE',
  };

  assert.throws(
    () => fs.copyFile(existingFile, nonexistentFile, -1, () => {}),
    validateError
  );
  assert.throws(
    () => fs.copyFileSync(existingFile, nonexistentFile, -1),
    validateError
  );
}

// copyFile: destination exists but the COPYFILE_EXCL flag is provided.
{
  const validateError = (err) => {
    if (err.code === 'ENOENT') {  // Could be ENOENT or EEXIST
      assert.strictEqual(err.message,
                         'ENOENT: no such file or directory, copyfile ' +
                         `'${existingFile}' -> '${existingFile2}'`);
      assert.strictEqual(err.errno, UV_ENOENT);
      assert.strictEqual(err.code, 'ENOENT');
      assert.strictEqual(err.syscall, 'copyfile');
    } else {
      assert.strictEqual(err.message,
                         'EEXIST: file already exists, copyfile ' +
                         `'${existingFile}' -> '${existingFile2}'`);
      assert.strictEqual(err.errno, UV_EEXIST);
      assert.strictEqual(err.code, 'EEXIST');
      assert.strictEqual(err.syscall, 'copyfile');
    }
    return true;
  };

  fs.copyFile(existingFile, existingFile2, COPYFILE_EXCL,
              common.mustCall(validateError));

  assert.throws(
    () => fs.copyFileSync(existingFile, existingFile2, COPYFILE_EXCL),
    validateError
  );
}

// copyFile: the source does not exist.
{
  const validateError = (err) => {
    assert.strictEqual(err.message,
                       'ENOENT: no such file or directory, copyfile ' +
                       `'${nonexistentFile}' -> '${existingFile2}'`);
    assert.strictEqual(err.errno, UV_ENOENT);
    assert.strictEqual(err.code, 'ENOENT');
    assert.strictEqual(err.syscall, 'copyfile');
    return true;
  };

  fs.copyFile(nonexistentFile, existingFile2, COPYFILE_EXCL,
              common.mustCall(validateError));

  assert.throws(
    () => fs.copyFileSync(nonexistentFile, existingFile2, COPYFILE_EXCL),
    validateError
  );
}

// read
{
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, read');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'read');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    const buf = Buffer.alloc(5);
    fs.read(fd, buf, 0, 1, 1, common.mustCall(validateError));

    assert.throws(
      () => fs.readSync(fd, buf, 0, 1, 1),
      validateError
    );
  });
}

// fchmod
{
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, fchmod');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'fchmod');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.fchmod(fd, 0o666, common.mustCall(validateError));

    assert.throws(
      () => fs.fchmodSync(fd, 0o666),
      validateError
    );
  });
}

// fchown
if (!common.isWindows) {
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, fchown');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'fchown');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.fchown(fd, process.getuid(), process.getgid(),
              common.mustCall(validateError));

    assert.throws(
      () => fs.fchownSync(fd, process.getuid(), process.getgid()),
      validateError
    );
  });
}

// write buffer
{
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, write');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'write');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    const buf = Buffer.alloc(5);
    fs.write(fd, buf, 0, 1, 1, common.mustCall(validateError));

    assert.throws(
      () => fs.writeSync(fd, buf, 0, 1, 1),
      validateError
    );
  });
}

// write string
{
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, write');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'write');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.write(fd, 'test', 1, common.mustCall(validateError));

    assert.throws(
      () => fs.writeSync(fd, 'test', 1),
      validateError
    );
  });
}


// futimes
if (!common.isAIX) {
  const validateError = (err) => {
    assert.strictEqual(err.message, 'EBADF: bad file descriptor, futime');
    assert.strictEqual(err.errno, UV_EBADF);
    assert.strictEqual(err.code, 'EBADF');
    assert.strictEqual(err.syscall, 'futime');
    return true;
  };

  common.runWithInvalidFD((fd) => {
    fs.futimes(fd, new Date(), new Date(), common.mustCall(validateError));

    assert.throws(
      () => fs.futimesSync(fd, new Date(), new Date()),
      validateError
    );
  });
}
