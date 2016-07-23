'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const expected = Buffer.from('hello');

common.refreshTmpDir();

// fs.write with all parameters provided:
{
  const filename = path.join(common.tmpDir, 'write1.txt');
  fs.open(filename, 'w', 0o644, common.mustCall(function(err, fd) {
    assert.ifError(err);

    const cb = common.mustCall(function(err, written) {
      assert.ifError(err);

      assert.strictEqual(expected.length, written);
      fs.closeSync(fd);

      var found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual(expected.toString(), found);
    });

    fs.write(fd, expected, 0, expected.length, null, cb);
  }));
}

// fs.write with a buffer, without the length parameter:
{
  const filename = path.join(common.tmpDir, 'write2.txt');
  fs.open(filename, 'w', 0o644, common.mustCall(function(err, fd) {
    assert.ifError(err);

    const cb = common.mustCall(function(err, written) {
      assert.ifError(err);

      assert.strictEqual(2, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual('lo', found);
    });

    fs.write(fd, Buffer.from('hello'), 3, cb);
  }));
}

// fs.write with a buffer, without the offset and length parameters:
{
  const filename = path.join(common.tmpDir, 'write3.txt');
  fs.open(filename, 'w', 0o644, common.mustCall(function(err, fd) {
    assert.ifError(err);

    const cb = common.mustCall(function(err, written) {
      assert.ifError(err);

      assert.strictEqual(expected.length, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual(expected.toString(), found);
    });

    fs.write(fd, expected, cb);
  }));
}

// fs.write with the offset passed as undefined followed by the callback:
{
  const filename = path.join(common.tmpDir, 'write4.txt');
  fs.open(filename, 'w', 0o644, common.mustCall(function(err, fd) {
    assert.ifError(err);

    const cb = common.mustCall(function(err, written) {
      assert.ifError(err);

      assert.strictEqual(expected.length, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual(expected.toString(), found);
    });

    fs.write(fd, expected, undefined, cb);
  }));
}

// fs.write with offset and length passed as undefined followed by the callback:
{
  const filename = path.join(common.tmpDir, 'write5.txt');
  fs.open(filename, 'w', 0o644, common.mustCall(function(err, fd) {
    assert.ifError(err);

    const cb = common.mustCall(function(err, written) {
      assert.ifError(err);

      assert.strictEqual(expected.length, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual(expected.toString(), found);
    });

    fs.write(fd, expected, undefined, undefined, cb);
  }));
}
