'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const expected = new ArrayBuffer(Buffer.byteLength('hello'));
Buffer.from(expected).write('hello');

common.refreshTmpDir();

// sync tests

// fs.writeSync with all parameters provided:
{
  const filename = path.join(common.tmpDir, 'write1.txt');
  const fd = fs.openSync(filename, 'w', 0o644);
  const written = fs.writeSync(fd, expected, 0, expected.byteLength, null);
  assert.strictEqual(expected.byteLength, written);
  fs.closeSync(fd);
  const found = fs.readFileSync(filename, 'utf8');
  assert.strictEqual(Buffer.from(expected).toString(), found);
}

// fs.writeSync with a buffer, without the length parameter:
{
  const filename = path.join(common.tmpDir, 'write2.txt');
  const fd = fs.openSync(filename, 'w', 0o644);
  const written = fs.writeSync(fd, expected, 3);
  assert.strictEqual(2, written);
  fs.closeSync(fd);
  const found = fs.readFileSync(filename, 'utf8');
  assert.strictEqual('lo', found);
}

// fs.writeSync with a buffer, without the offset and length parameters:
{
  const filename = path.join(common.tmpDir, 'write3.txt');
  const fd = fs.openSync(filename, 'w', 0o644);
  const written = fs.writeSync(fd, expected);
  assert.strictEqual(expected.byteLength, written);
  fs.closeSync(fd);
  const found = fs.readFileSync(filename, 'utf8');
  assert.deepStrictEqual(Buffer.from(expected).toString(), found);
}

// fs.writeSync with the offset passed as undefined:
{
  const filename = path.join(common.tmpDir, 'write4.txt');
  const fd = fs.openSync(filename, 'w', 0o644);
  const written = fs.writeSync(fd, expected, undefined);
  assert.strictEqual(expected.byteLength, written);
  fs.closeSync(fd);
  const found = fs.readFileSync(filename, 'utf8');
  assert.deepStrictEqual(Buffer.from(expected).toString(), found);
}

// fs.writeSync with offset and length passed as undefined:
{
  const filename = path.join(common.tmpDir, 'write5.txt');
  const fd = fs.openSync(filename, 'w', 0o644);
  const written = fs.writeSync(fd, expected, undefined, undefined);
  assert.strictEqual(expected.byteLength, written);
  fs.closeSync(fd);
  const found = fs.readFileSync(filename, 'utf8');
  assert.strictEqual(Buffer.from(expected).toString(), found);
}

// async tests

// fs.write with all parameters provided:
{
  const filename = path.join(common.tmpDir, 'write1.txt');
  fs.open(filename, 'w', 0o644, common.mustCall((err, fd) => {
    assert.ifError(err);

    const cb = common.mustCall((err, written) => {
      assert.ifError(err);

      assert.strictEqual(expected.byteLength, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.strictEqual(Buffer.from(expected).toString(), found);
    });

    fs.write(fd, expected, 0, expected.byteLength, null, cb);
  }));
}

// fs.write with a buffer, without the length parameter:
{
  const filename = path.join(common.tmpDir, 'write2.txt');
  fs.open(filename, 'w', 0o644, common.mustCall((err, fd) => {
    assert.ifError(err);

    const cb = common.mustCall((err, written) => {
      assert.ifError(err);

      assert.strictEqual(2, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.strictEqual('lo', found);
    });

    fs.write(fd, expected, 3, cb);
  }));
}

// fs.write with a buffer, without the offset and length parameters:
{
  const filename = path.join(common.tmpDir, 'write3.txt');
  fs.open(filename, 'w', 0o644, common.mustCall(function(err, fd) {
    assert.ifError(err);

    const cb = common.mustCall(function(err, written) {
      assert.ifError(err);

      assert.strictEqual(expected.byteLength, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual(Buffer.from(expected).toString(), found);
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

      assert.strictEqual(expected.byteLength, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.deepStrictEqual(Buffer.from(expected).toString(), found);
    });

    fs.write(fd, expected, undefined, cb);
  }));
}

// fs.write with offset and length passed as undefined followed by the callback:
{
  const filename = path.join(common.tmpDir, 'write5.txt');
  fs.open(filename, 'w', 0o644, common.mustCall((err, fd) => {
    assert.ifError(err);

    const cb = common.mustCall((err, written) => {
      assert.ifError(err);

      assert.strictEqual(expected.byteLength, written);
      fs.closeSync(fd);

      const found = fs.readFileSync(filename, 'utf8');
      assert.strictEqual(Buffer.from(expected).toString(), found);
    });

    fs.write(fd, expected, undefined, undefined, cb);
  }));
}
