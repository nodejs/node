'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const fsp = require('fs').promises;
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// fs.writev with buffers totalling > INT32_MAX should throw ERR_OUT_OF_RANGE
// Refs: https://github.com/nodejs/node/issues/40779
// Precedent: test/parallel/test-fs-write-buffer-large.js (c4e7dca8f30)

common.skipIf32Bits();

let buf;
try {
  buf = Buffer.allocUnsafe(0x7FFFFFFF + 1);
} catch (e) {
  if (e.message !== 'Array buffer allocation failed') throw e;
  common.skip('skipped due to memory requirements');
}

const filename = tmpdir.resolve('writev-large.txt');
const filename2 = tmpdir.resolve('writev-large2.txt');
const filename3 = tmpdir.resolve('writev-large3.txt');

// writevSync throws synchronously
{
  const fd = fs.openSync(filename, 'w');
  assert.throws(() => {
    fs.writevSync(fd, [buf], 0);
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
    message: /The value of "length" is out of range.*2147483648/,
  });
  // Two buffers that sum to > kIoMaxLength also throw
  const small = Buffer.allocUnsafe(10);
  assert.throws(() => {
    fs.writevSync(fd, [buf, small], 0);
  }, {
    code: 'ERR_OUT_OF_RANGE',
  });
  fs.closeSync(fd);
}

// writev (callback) throws synchronously before the syscall
{
  const fd = fs.openSync(filename2, 'w');
  assert.throws(() => {
    fs.writev(fd, [buf], 0, common.mustNotCall());
  }, {
    code: 'ERR_OUT_OF_RANGE',
    name: 'RangeError',
  });
  assert.throws(() => {
    fs.writev(fd, [buf, Buffer.allocUnsafe(1)], common.mustNotCall());
  }, {
    code: 'ERR_OUT_OF_RANGE',
  });
  fs.closeSync(fd);
}

// fs.promises.writev rejects with ERR_OUT_OF_RANGE
(async () => {
  const handle = await fsp.open(filename3, 'w');
  await assert.rejects(
    handle.writev([buf], 0),
    {
      code: 'ERR_OUT_OF_RANGE',
      name: 'RangeError',
    }
  );
  await assert.rejects(
    handle.writev([buf, Buffer.allocUnsafe(1)]),
    {
      code: 'ERR_OUT_OF_RANGE',
    }
  );
  // Empty array still succeeds
  const { bytesWritten } = await handle.writev([], null);
  assert.strictEqual(bytesWritten, 0);
  await handle.close();
})().then(common.mustCall());
