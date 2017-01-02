'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const tmp = common.tmpDir;
const filename = path.resolve(tmp, 'truncate-file.txt');
const data = Buffer.alloc(1024 * 16, 'x');

common.refreshTmpDir();

let stat;

// truncateSync
fs.writeFileSync(filename, data);
stat = fs.statSync(filename);
assert.strictEqual(stat.size, 1024 * 16);

fs.truncateSync(filename, 1024);
stat = fs.statSync(filename);
assert.strictEqual(stat.size, 1024);

fs.truncateSync(filename);
stat = fs.statSync(filename);
assert.strictEqual(stat.size, 0);

// ftruncateSync
fs.writeFileSync(filename, data);
const fd = fs.openSync(filename, 'r+');

stat = fs.statSync(filename);
assert.strictEqual(stat.size, 1024 * 16);

fs.ftruncateSync(fd, 1024);
stat = fs.statSync(filename);
assert.strictEqual(stat.size, 1024);

fs.ftruncateSync(fd);
stat = fs.statSync(filename);
assert.strictEqual(stat.size, 0);

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
      assert.strictEqual(stat.size, 1024 * 16);

      fs.truncate(filename, 1024, function(er) {
        if (er) return cb(er);
        fs.stat(filename, function(er, stat) {
          if (er) return cb(er);
          assert.strictEqual(stat.size, 1024);

          fs.truncate(filename, function(er) {
            if (er) return cb(er);
            fs.stat(filename, function(er, stat) {
              if (er) return cb(er);
              assert.strictEqual(stat.size, 0);
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
      assert.strictEqual(stat.size, 1024 * 16);

      fs.open(filename, 'w', function(er, fd) {
        if (er) return cb(er);
        fs.ftruncate(fd, 1024, function(er) {
          if (er) return cb(er);
          fs.stat(filename, function(er, stat) {
            if (er) return cb(er);
            assert.strictEqual(stat.size, 1024);

            fs.ftruncate(fd, function(er) {
              if (er) return cb(er);
              fs.stat(filename, function(er, stat) {
                if (er) return cb(er);
                assert.strictEqual(stat.size, 0);
                fs.close(fd, cb);
              });
            });
          });
        });
      });
    });
  });
}


// Make sure if the size of the file is smaller than the length then it is
// filled with zeroes.

{
  const file1 = path.resolve(tmp, 'truncate-file-1.txt');
  fs.writeFileSync(file1, 'Hi');
  fs.truncateSync(file1, 4);
  assert(fs.readFileSync(file1).equals(Buffer.from('Hi\u0000\u0000')));
}

{
  const file2 = path.resolve(tmp, 'truncate-file-2.txt');
  fs.writeFileSync(file2, 'Hi');
  const fd = fs.openSync(file2, 'r+');
  process.on('exit', () => fs.closeSync(fd));
  fs.ftruncateSync(fd, 4);
  assert(fs.readFileSync(file2).equals(Buffer.from('Hi\u0000\u0000')));
}

{
  const file3 = path.resolve(tmp, 'truncate-file-3.txt');
  fs.writeFileSync(file3, 'Hi');
  fs.truncate(file3, 4, common.mustCall(function(err) {
    assert.ifError(err);
    assert(fs.readFileSync(file3).equals(Buffer.from('Hi\u0000\u0000')));
  }));
}

{
  const file4 = path.resolve(tmp, 'truncate-file-4.txt');
  fs.writeFileSync(file4, 'Hi');
  const fd = fs.openSync(file4, 'r+');
  process.on('exit', () => fs.closeSync(fd));
  fs.ftruncate(fd, 4, common.mustCall(function(err) {
    assert.ifError(err);
    assert(fs.readFileSync(file4).equals(Buffer.from('Hi\u0000\u0000')));
  }));
}
