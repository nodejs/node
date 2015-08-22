'use strict';
var common = require('../common');
var assert = require('assert');
var util = require('util');
var fs = require('fs');

var tests_ok = 0;
var tests_run = 0;

function stat_resource(resource) {
  if (typeof resource == 'string') {
    return fs.statSync(resource);
  } else {
    // ensure mtime has been written to disk
    fs.fsyncSync(resource);
    return fs.fstatSync(resource);
  }
}

function check_mtime(resource, mtime) {
  mtime = fs._toUnixTimestamp(mtime);
  var stats = stat_resource(resource);
  var real_mtime = fs._toUnixTimestamp(stats.mtime);
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

// the tests assume that __filename belongs to the user running the tests
// this should be a fairly safe assumption; testing against a temp file
// would be even better though (node doesn't have such functionality yet)
function runTest(atime, mtime, callback) {

  var fd;
  //
  // test synchronized code paths, these functions throw on failure
  //
  function syncTests() {
    fs.utimesSync(__filename, atime, mtime);
    expect_ok('utimesSync', __filename, undefined, atime, mtime);
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

    var err;
    err = undefined;
    try {
      fs.utimesSync('foobarbaz', atime, mtime);
    } catch (ex) {
      err = ex;
    }
    expect_errno('utimesSync', 'foobarbaz', err, 'ENOENT');
    tests_run++;
  }

  //
  // test async code paths
  //
  fs.utimes(__filename, atime, mtime, function(err) {
    expect_ok('utimes', __filename, err, atime, mtime);

    fs.utimes('foobarbaz', atime, mtime, function(err) {
      expect_errno('utimes', 'foobarbaz', err, 'ENOENT');

      // don't close this fd
      if (common.isWindows) {
        fd = fs.openSync(__filename, 'r+');
      } else {
        fd = fs.openSync(__filename, 'r');
      }

      fs.futimes(fd, atime, mtime, function(err) {
        expect_ok('futimes', fd, err, atime, mtime);
        syncTests();
        callback();
      });
      tests_run++;
    });
    tests_run++;
  });
  tests_run++;
}

var stats = fs.statSync(__filename);

// run tests
runTest(new Date('1982-09-10 13:37'), new Date('1982-09-10 13:37'), function() {
  runTest(new Date(), new Date(), function() {
    runTest(123456.789, 123456.789, function() {
      runTest(stats.mtime, stats.mtime, function() {
        runTest(NaN, Infinity, function() {
          runTest('123456', -1, function() {
            // done
          });
        });
      });
    });
  });
});

// validate if the file descriptors are in the valid range
assert.throws(() => fs.futimes(-1, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync(-1),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes('-1', () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync('-1'),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes(0xFFFFFFFF + 1, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync(0xFFFFFFFF + 1),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes('0x100000000', () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync('0x100000000'),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
// invalid file descriptors also should throw
assert.throws(() => fs.futimes(null, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync(null),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes(undefined, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync(undefined),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes(NaN, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync(NaN),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes({}, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync({}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes([], () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync([]),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes(() => true, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync(() => true),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes(() => /RegEx/, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync(() => /RegEx/),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimes(() => 3.14, () => {}),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);
assert.throws(() => fs.futimesSync(() => 3.14),
              /TypeError: file descriptor must be a unsigned 32-bit integer/);

process.on('exit', function() {
  console.log('Tests run / ok:', tests_run, '/', tests_ok);
  assert.equal(tests_ok, tests_run);
});
