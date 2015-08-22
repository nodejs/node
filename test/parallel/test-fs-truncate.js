'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var tmp = common.tmpDir;
var filename = path.resolve(tmp, 'truncate-file.txt');
var data = Buffer.alloc(1024 * 16, 'x');

common.refreshTmpDir();

var stat;

// truncateSync
fs.writeFileSync(filename, data);
stat = fs.statSync(filename);
assert.equal(stat.size, 1024 * 16);

fs.truncateSync(filename, 1024);
stat = fs.statSync(filename);
assert.equal(stat.size, 1024);

fs.truncateSync(filename);
stat = fs.statSync(filename);
assert.equal(stat.size, 0);

// path must be a string
assert.throws(function() {
  fs.truncateSync({});
}, /path must be a string/);

assert.throws(function() {
  fs.truncateSync([]);
}, /path must be a string/);

// Even invalid file descriptors are not allowed
assert.throws(function() {
  fs.truncateSync(-1);
}, /path must be a string/);

assert.throws(function() {
  fs.truncateSync(NaN);
}, /path must be a string/);

assert.throws(function() {
  fs.truncateSync(Infinity);
}, /path must be a string/);

// Invalid lengths will also fail
assert.throws(function() {
  fs.truncateSync('', '');
}, /length must be a positive integer/);

assert.throws(function() {
  fs.truncateSync('', -1);
}, /length must be a positive integer/);

assert.throws(function() {
  fs.truncateSync('', NaN);
}, /length must be a positive integer/);

assert.throws(function() {
  fs.truncateSync('', Infinity);
}, /length must be a positive integer/);

// null is a special case which will also be treated as zero length
fs.writeFileSync(filename, data);

stat = fs.statSync(filename);
assert.equal(stat.size, 1024 * 16);

fs.truncateSync(filename, null);
stat = fs.statSync(filename);
assert.equal(stat.size, 0);

// ftruncateSync
fs.writeFileSync(filename, data);
var fd = fs.openSync(filename, 'r+');

stat = fs.statSync(filename);
assert.equal(stat.size, 1024 * 16);

fs.ftruncateSync(fd, 1024);
stat = fs.statSync(filename);
assert.equal(stat.size, 1024);

fs.ftruncateSync(fd);
stat = fs.statSync(filename);
assert.equal(stat.size, 0);

fs.closeSync(fd);

// file descriptor must be a unsigned 32-bit integer
assert.throws(function() {
  fs.ftruncateSync({});
}, /file descriptor must be a unsigned 32-bit integer/);

// Even invalid file descriptors are not allowed
assert.throws(function() {
  fs.ftruncateSync(-1);
}, /file descriptor must be a unsigned 32-bit integer/);

assert.throws(function() {
  fs.ftruncateSync(NaN);
}, /file descriptor must be a unsigned 32-bit integer/);

assert.throws(function() {
  fs.ftruncateSync(Infinity);
}, /file descriptor must be a unsigned 32-bit integer/);

// Invalid lengths will also fail
assert.throws(function() {
  fs.ftruncateSync(1, '');
}, /length must be a positive integer/);

assert.throws(function() {
  fs.ftruncateSync(1, -1);
}, /length must be a positive integer/);

assert.throws(function() {
  fs.ftruncateSync(1, NaN);
}, /length must be a positive integer/);

assert.throws(function() {
  fs.ftruncateSync(1, Infinity);
}, /length must be a positive integer/);

// null is a special case which will also be treated as zero length
fs.writeFileSync(filename, data);
fd = fs.openSync(filename, 'r+');

fs.ftruncateSync(fd, null);
stat = fs.statSync(filename);
assert.equal(stat.size, 0);

fs.closeSync(fd);

// async tests
testTruncate(common.mustCall(function(er) {
  if (er) throw er;
  testFtruncate(common.mustCall(function(er) {
    if (er) throw er;
  }));
}));

function testTruncate(cb) {
  fs.writeFile(filename, data, function(er) {
    if (er) return cb(er);
    fs.stat(filename, function(er, stat) {
      if (er) return cb(er);
      assert.equal(stat.size, 1024 * 16);

      fs.truncate(filename, 1024, function(er) {
        if (er) return cb(er);
        fs.stat(filename, function(er, stat) {
          if (er) return cb(er);
          assert.equal(stat.size, 1024);

          fs.truncate(filename, function(er) {
            if (er) return cb(er);
            fs.stat(filename, function(er, stat) {
              if (er) return cb(er);
              assert.equal(stat.size, 0);
              cb();
            });
          });
        });
      });
    });
  });
}


function testFtruncate(cb) {
  fs.writeFile(filename, data, function(er) {
    if (er) return cb(er);
    fs.stat(filename, function(er, stat) {
      if (er) return cb(er);
      assert.equal(stat.size, 1024 * 16);

      fs.open(filename, 'w', function(er, fd) {
        if (er) return cb(er);
        fs.ftruncate(fd, 1024, function(er) {
          if (er) return cb(er);
          fs.stat(filename, function(er, stat) {
            if (er) return cb(er);
            assert.equal(stat.size, 1024);

            fs.ftruncate(fd, function(er) {
              if (er) return cb(er);
              fs.stat(filename, function(er, stat) {
                if (er) return cb(er);
                assert.equal(stat.size, 0);
                fs.close(fd, cb);
              });
            });
          });
        });
      });
    });
  });
}
