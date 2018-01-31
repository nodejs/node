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

common.refreshTmpDir();

let tests_ok = 0;
let tests_run = 0;

function stat_resource(resource) {
  if (typeof resource === 'string') {
    return fs.statSync(resource);
  } else {
    // ensure mtime has been written to disk
    fs.fsyncSync(resource);
    return fs.fstatSync(resource);
  }
}

function check_mtime(resource, mtime) {
  mtime = fs._toUnixTimestamp(mtime);
  const stats = stat_resource(resource);
  const real_mtime = fs._toUnixTimestamp(stats.mtime);
  // check up to single-second precision
  // sub-second precision is OS and fs dependant
  return mtime - real_mtime < 2;
}

function expect_errno(syscall, resource, err, errno) {
  if (err && (err.code === errno || err.code === 'ENOSYS')) {
    tests_ok++;
  } else {
    console.log('FAILED:', 'expect_errno', util.inspect(arguments));
  }
}

function expect_ok(syscall, resource, err, atime, mtime) {
  if (!err && check_mtime(resource, mtime) ||
      err && err.code === 'ENOSYS') {
    tests_ok++;
  } else {
    console.log('FAILED:', 'expect_ok', util.inspect(arguments));
  }
}

function testIt(atime, mtime, callback) {

  let fd;
  //
  // test synchronized code paths, these functions throw on failure
  //
  function syncTests() {
    fs.utimesSync(common.tmpDir, atime, mtime);
    expect_ok('utimesSync', common.tmpDir, undefined, atime, mtime);
    tests_run++;

    // some systems don't have futimes
    // if there's an error, it should be ENOSYS
    try {
      tests_run++;
      fs.futimesSync(fd, atime, mtime);
      expect_ok('futimesSync', fd, undefined, atime, mtime);
    } catch (ex) {
      expect_errno('futimesSync', fd, ex, 'ENOSYS');
    }

    let err = undefined;
    try {
      fs.utimesSync('foobarbaz', atime, mtime);
    } catch (ex) {
      err = ex;
    }
    expect_errno('utimesSync', 'foobarbaz', err, 'ENOENT');
    tests_run++;

    err = undefined;
    common.expectsError(
      () => fs.futimesSync(-1, atime, mtime),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError
      }
    );
    tests_run++;
  }

  //
  // test async code paths
  //
  fs.utimes(common.tmpDir, atime, mtime, common.mustCall(function(err) {
    expect_ok('utimes', common.tmpDir, err, atime, mtime);

    fs.utimes('foobarbaz', atime, mtime, common.mustCall(function(err) {
      expect_errno('utimes', 'foobarbaz', err, 'ENOENT');

      // don't close this fd
      if (common.isWindows) {
        fd = fs.openSync(common.tmpDir, 'r+');
      } else {
        fd = fs.openSync(common.tmpDir, 'r');
      }

      fs.futimes(fd, atime, mtime, common.mustCall(function(err) {
        expect_ok('futimes', fd, err, atime, mtime);

        common.expectsError(
          () => fs.futimes(-1, atime, mtime, common.mustNotCall()),
          {
            code: 'ERR_INVALID_ARG_TYPE',
            type: TypeError,
          }
        );

        syncTests();

        tests_run++;
      }));
      tests_run++;
    }));
    tests_run++;
  }));
  tests_run++;
}

const stats = fs.statSync(common.tmpDir);

// run tests
const runTest = common.mustCall(testIt, 1);

runTest(new Date('1982-09-10 13:37'), new Date('1982-09-10 13:37'), function() {
  runTest(new Date(), new Date(), function() {
    runTest(123456.789, 123456.789, function() {
      runTest(stats.mtime, stats.mtime, function() {
        runTest('123456', -1, function() {
          runTest(
            new Date('2017-04-08T17:59:38.008Z'),
            new Date('2017-04-08T17:59:38.008Z'),
            common.mustCall(function() {
              // done
            })
          );
        });
      });
    });
  });
});

process.on('exit', function() {
  assert.strictEqual(tests_ok, tests_run - 2);
});


// Ref: https://github.com/nodejs/node/issues/13255
const path = `${common.tmpDir}/test-utimes-precision`;
fs.writeFileSync(path, '');

// test Y2K38 for all platforms [except 'arm', and 'SunOS']
if (!process.arch.includes('arm') && !common.isSunOS) {
  // because 2 ** 31 doesn't look right
  // eslint-disable-next-line space-infix-ops
  const Y2K38_mtime = 2**31;
  fs.utimesSync(path, Y2K38_mtime, Y2K38_mtime);
  const Y2K38_stats = fs.statSync(path);
  assert.strictEqual(Y2K38_mtime, Y2K38_stats.mtime.getTime() / 1000);
}

if (common.isWindows) {
  // this value would get converted to (double)1713037251359.9998
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

[false, 0, {}, [], null, undefined].forEach((i) => {
  common.expectsError(
    () => fs.utimes(i, new Date(), new Date(), common.mustNotCall()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
  common.expectsError(
    () => fs.utimesSync(i, new Date(), new Date()),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError
    }
  );
});
