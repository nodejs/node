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
const assert = require('assert');
const util = require('util');
const fs = require('fs');
const url = require('url');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const lpath = `${tmpdir.path}/symlink`;
fs.symlinkSync('unoent-entry', lpath);

function stat_resource(resource, statSync = fs.statSync) {
  if (typeof resource === 'string') {
    return statSync(resource);
  }
  const stats = fs.fstatSync(resource);
  // Ensure mtime has been written to disk
  // except for directories on AIX where it cannot be synced
  if ((common.isAIX || common.isIBMi) && stats.isDirectory())
    return stats;
  fs.fsyncSync(resource);
  return fs.fstatSync(resource);
}

function check_mtime(resource, mtime, statSync) {
  mtime = fs._toUnixTimestamp(mtime);
  const stats = stat_resource(resource, statSync);
  const real_mtime = fs._toUnixTimestamp(stats.mtime);
  return mtime - real_mtime;
}

function expect_errno(syscall, resource, err, errno) {
  assert(
    err && (err.code === errno || err.code === 'ENOSYS'),
    `FAILED: expect_errno ${util.inspect(arguments)}`
  );
}

function expect_ok(syscall, resource, err, atime, mtime, statSync) {
  const mtime_diff = check_mtime(resource, mtime, statSync);
  assert(
    // Check up to single-second precision.
    // Sub-second precision is OS and fs dependant.
    !err && (mtime_diff < 2) || err && err.code === 'ENOSYS',
    `FAILED: expect_ok ${util.inspect(arguments)}
     check_mtime: ${mtime_diff}`
  );
}

const stats = fs.statSync(tmpdir.path);

const asPath = (path) => path;
const asUrl = (path) => url.pathToFileURL(path);

const cases = [
  [asPath, new Date('1982-09-10 13:37')],
  [asPath, new Date()],
  [asPath, 123456.789],
  [asPath, stats.mtime],
  [asPath, '123456', -1],
  [asPath, new Date('2017-04-08T17:59:38.008Z')],
  [asUrl, new Date()],
];

runTests(cases.values());

function runTests(iter) {
  const { value, done } = iter.next();
  if (done) return;

  // Support easy setting same or different atime / mtime values.
  const [pathType, atime, mtime = atime] = value;

  let fd;
  //
  // test async code paths
  //
  fs.utimes(pathType(tmpdir.path), atime, mtime, common.mustCall((err) => {
    expect_ok('utimes', tmpdir.path, err, atime, mtime);

    fs.lutimes(pathType(lpath), atime, mtime, common.mustCall((err) => {
      expect_ok('lutimes', lpath, err, atime, mtime, fs.lstatSync);

      fs.utimes(pathType('foobarbaz'), atime, mtime, common.mustCall((err) => {
        expect_errno('utimes', 'foobarbaz', err, 'ENOENT');

        // don't close this fd
        if (common.isWindows) {
          fd = fs.openSync(tmpdir.path, 'r+');
        } else {
          fd = fs.openSync(tmpdir.path, 'r');
        }

        fs.futimes(fd, atime, mtime, common.mustCall((err) => {
          expect_ok('futimes', fd, err, atime, mtime);

          syncTests();

          setImmediate(common.mustCall(runTests), iter);
        }));
      }));
    }));
  }));

  //
  // test synchronized code paths, these functions throw on failure
  //
  function syncTests() {
    fs.utimesSync(pathType(tmpdir.path), atime, mtime);
    expect_ok('utimesSync', tmpdir.path, undefined, atime, mtime);

    fs.lutimesSync(pathType(lpath), atime, mtime);
    expect_ok('lutimesSync', lpath, undefined, atime, mtime, fs.lstatSync);

    // Some systems don't have futimes
    // if there's an error, it should be ENOSYS
    try {
      fs.futimesSync(fd, atime, mtime);
      expect_ok('futimesSync', fd, undefined, atime, mtime);
    } catch (ex) {
      expect_errno('futimesSync', fd, ex, 'ENOSYS');
    }

    let err;
    try {
      fs.utimesSync(pathType('foobarbaz'), atime, mtime);
    } catch (ex) {
      err = ex;
    }
    expect_errno('utimesSync', 'foobarbaz', err, 'ENOENT');

    err = undefined;
  }
}

const expectTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError'
};
// utimes-only error cases
{
  assert.throws(
    () => fs.utimes(0, new Date(), new Date(), common.mustNotCall()),
    expectTypeError
  );
  assert.throws(
    () => fs.utimesSync(0, new Date(), new Date()),
    expectTypeError
  );
}

// shared error cases
[false, {}, [], null, undefined].forEach((i) => {
  assert.throws(
    () => fs.utimes(i, new Date(), new Date(), common.mustNotCall()),
    expectTypeError
  );
  assert.throws(
    () => fs.utimesSync(i, new Date(), new Date()),
    expectTypeError
  );
  assert.throws(
    () => fs.futimes(i, new Date(), new Date(), common.mustNotCall()),
    expectTypeError
  );
  assert.throws(
    () => fs.futimesSync(i, new Date(), new Date()),
    expectTypeError
  );
});

const expectRangeError = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
  message: 'The value of "fd" is out of range. ' +
           'It must be >= 0 && <= 2147483647. Received -1'
};
// futimes-only error cases
{
  assert.throws(
    () => fs.futimes(-1, new Date(), new Date(), common.mustNotCall()),
    expectRangeError
  );
  assert.throws(
    () => fs.futimesSync(-1, new Date(), new Date()),
    expectRangeError
  );
}
