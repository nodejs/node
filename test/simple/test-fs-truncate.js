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

var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var tmp = common.tmpDir;
var filename = path.resolve(tmp, 'truncate-file.txt');
var data = new Buffer(1024 * 16);
data.fill('x');

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

// async tests
var success = 0;
testTruncate(function(er) {
  if (er) throw er;
  success++;
  testFtruncate(function(er) {
    if (er) throw er;
    success++;
  });
});

process.on('exit', function() {
  assert.equal(success, 2);
  console.log('ok');
});

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
