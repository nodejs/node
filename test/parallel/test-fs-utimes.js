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

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

function stat_resource(resource) {
  if (typeof resource === 'string') {
    return fs.statSync(resource);
  } else {
    const stats = fs.fstatSync(resource);
    // ensure mtime has been written to disk
    // except for directories on AIX where it cannot be synced
    if (common.isAIX && stats.isDirectory())
      return stats;
    fs.fsyncSync(resource);
    return fs.fstatSync(resource);
  }
}

function check_mtime(resource, mtime) {
  mtime = fs._toUnixTimestamp(mtime);
  const stats = stat_resource(resource);
  const real_mtime = fs._toUnixTimestamp(stats.mtime);
  return mtime - real_mtime;
}

function expect_errno(syscall, resource, err, errno) {
  assert(
    err && (err.code === errno || err.code === 'ENOSYS'),
    `FAILED: expect_errno ${util.inspect(arguments)}`
  );
}

function expect_ok(syscall, resource, err, atime, mtime) {
  const mtime_diff = check_mtime(resource, mtime);
  assert(
    // check up to single-second precision
    // sub-second precision is OS and fs dependant
    !err && (mtime_diff < 2) || err && err.code === 'ENOSYS',
    `FAILED: expect_ok ${util.inspect(arguments)}
     check_mtime: ${mtime_diff}`
  );
}

const stats = fs.statSync(tmpdir.path);

const cases = [
  new Date('1982-09-10 13:37'),
  new Date(),
  123456.789,
  stats.mtime,
  ['123456', -1],
  new Date('2017-04-08T17:59:38.008Z')
];
runTests(cases.values());

function runTests(iter) {
  const { value, done } = iter.next();
  if (done) return;
  // Support easy setting same or different atime / mtime values
  const [atime, mtime] = Array.isArray(value) ? value : [value, value];

  let fd;
  //
  // test async code paths
  //
  fs.utimes(tmpdir.path, atime, mtime, common.mustCall((err) => {
    expect_ok('utimes', tmpdir.path, err, atime, mtime);

    fs.utimes('foobarbaz', atime, mtime, common.mustCall((err) => {
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

  //
  // test synchronized code paths, these functions throw on failure
  //
  function syncTests() {
    fs.utimesSync(tmpdir.path, atime, mtime);
    expect_ok('utimesSync', tmpdir.path, undefined, atime, mtime);

    // some systems don't have futimes
    // if there's an error, it should be ENOSYS
    try {
      fs.futimesSync(fd, atime, mtime);
      expect_ok('futimesSync', fd, undefined, atime, mtime);
    } catch (ex) {
      expect_errno('futimesSync', fd, ex, 'ENOSYS');
    }

    let err;
    try {
      fs.utimesSync('foobarbaz', atime, mtime);
    } catch (ex) {
      err = ex;
    }
    expect_errno('utimesSync', 'foobarbaz', err, 'ENOENT');

    err = undefined;
  }
}

// Ref: https://github.com/nodejs/node/issues/13255
const path = `${tmpdir.path}/test-utimes-precision`;
fs.writeFileSync(path, '');

// Test Y2K38 for all platforms [except 'arm', 'OpenBSD' and 'SunOS']
if (!process.arch.includes('arm') && !common.isOpenBSD && !common.isSunOS) {
  const Y2K38_mtime = 2 ** 31;
  fs.utimesSync(path, Y2K38_mtime, Y2K38_mtime);
  const Y2K38_stats = fs.statSync(path);
  assert.strictEqual(Y2K38_mtime, Y2K38_stats.mtime.getTime() / 1000);
}

if (common.isWindows) {
  // This value would get converted to (double)1713037251359.9998
  const truncate_mtime = 1713037251360;
  fs.utimesSync(path, truncate_mtime / 1000, truncate_mtime / 1000);
  const truncate_stats = fs.statSync(path);
  assert.strictEqual(truncate_mtime, truncate_stats.mtime.getTime());

  // test Y2K38 for windows
  // This value if treaded as a `signed long` gets converted to -2135622133469.
  // POSIX systems stores timestamps in {long t_sec, long t_usec}.
  // NTFS stores times in nanoseconds in a single `uint64_t`, so when libuv
  // calculates (long)`uv_timespec_t.tv_sec` we get 2's complement.
  const overflow_mtime = 2159345162531;
  fs.utimesSync(path, overflow_mtime / 1000, overflow_mtime / 1000);
  const overflow_stats = fs.statSync(path);
  assert.strictEqual(overflow_mtime, overflow_stats.mtime.getTime());
}

const expectTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError
};
// utimes-only error cases
{
  common.expectsError(
    () => fs.utimes(0, new Date(), new Date(), common.mustNotCall()),
    expectTypeError
  );
  common.expectsError(
    () => fs.utimesSync(0, new Date(), new Date()),
    expectTypeError
  );
}

// shared error cases
[false, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.utimes(i, new Date(), new Date(), common.mustNotCall()),
    expectTypeError
  );
  common.expectsError(
    () => fs.utimesSync(i, new Date(), new Date()),
    expectTypeError
  );
  common.expectsError(
    () => fs.futimes(i, new Date(), new Date(), common.mustNotCall()),
    expectTypeError
  );
  common.expectsError(
    () => fs.futimesSync(i, new Date(), new Date()),
    expectTypeError
  );
});

const expectRangeError = {
  code: 'ERR_OUT_OF_RANGE',
  type: RangeError,
  message: 'The value of "fd" is out of range. ' +
           'It must be >= 0 && < 4294967296. Received -1'
};
// futimes-only error cases
{
  common.expectsError(
    () => fs.futimes(-1, new Date(), new Date(), common.mustNotCall()),
    expectRangeError
  );
  common.expectsError(
    () => fs.futimesSync(-1, new Date(), new Date()),
    expectRangeError
  );
}
