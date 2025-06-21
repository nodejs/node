'use strict';
const common = require('../common');

// Test fs.readFile using a file descriptor.

const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const fn = fixtures.path('empty.txt');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

tempFd(function(fd, close) {
  fs.readFile(fd, function(err, data) {
    assert.ok(data);
    close();
  });
});

tempFd(function(fd, close) {
  fs.readFile(fd, 'utf8', function(err, data) {
    assert.strictEqual(data, '');
    close();
  });
});

tempFdSync(function(fd) {
  assert.ok(fs.readFileSync(fd));
});

tempFdSync(function(fd) {
  assert.strictEqual(fs.readFileSync(fd, 'utf8'), '');
});

function tempFd(callback) {
  fs.open(fn, 'r', function(err, fd) {
    assert.ifError(err);
    callback(fd, function() {
      fs.close(fd, function(err) {
        assert.ifError(err);
      });
    });
  });
}

function tempFdSync(callback) {
  const fd = fs.openSync(fn, 'r');
  callback(fd);
  fs.closeSync(fd);
}

{
  // This test makes sure that `readFile()` always reads from the current
  // position of the file, instead of reading from the beginning of the file,
  // when used with file descriptors.

  const filename = tmpdir.resolve('test.txt');
  fs.writeFileSync(filename, 'Hello World');

  {
    // Tests the fs.readFileSync().
    const fd = fs.openSync(filename, 'r');

    // Read only five bytes, so that the position moves to five.
    const buf = Buffer.alloc(5);
    assert.strictEqual(fs.readSync(fd, buf, 0, 5), 5);
    assert.strictEqual(buf.toString(), 'Hello');

    // readFileSync() should read from position five, instead of zero.
    assert.strictEqual(fs.readFileSync(fd).toString(), ' World');

    fs.closeSync(fd);
  }

  {
    // Tests the fs.readFile().
    fs.open(filename, 'r', common.mustSucceed((fd) => {
      const buf = Buffer.alloc(5);

      // Read only five bytes, so that the position moves to five.
      fs.read(fd, buf, 0, 5, null, common.mustSucceed((bytes) => {
        assert.strictEqual(bytes, 5);
        assert.strictEqual(buf.toString(), 'Hello');

        fs.readFile(fd, common.mustSucceed((data) => {
          // readFile() should read from position five, instead of zero.
          assert.strictEqual(data.toString(), ' World');

          fs.closeSync(fd);
        }));
      }));
    }));
  }
}
