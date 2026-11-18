'use strict';

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const fsPromises = require('node:fs/promises');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const file = tmpdir.resolve('file');
const outOfBoundsRanges = [
  [0, 2],
  [2, 1],
];
const expectedError = {
  name: 'RangeError',
  code: 'ERR_OUT_OF_RANGE',
  message: 'buffer is out of bounds',
};

function createSpoofedBuffer() {
  const buffer = new Uint8Array(1);
  Object.defineProperty(buffer, 'byteLength', { value: 3 });
  return buffer;
}

fs.writeFileSync(file, 'test');

const fd = fs.openSync(file, 'r+');
for (const [offset, length] of outOfBoundsRanges) {
  assert.throws(
    () => fs.readSync(fd, createSpoofedBuffer(), offset, length, 0),
    expectedError,
  );
  assert.throws(
    () => fs.writeSync(fd, createSpoofedBuffer(), offset, length, 0),
    expectedError,
  );
  assert.throws(
    () => fs.read(fd, createSpoofedBuffer(), offset, length, 0,
                  common.mustNotCall()),
    expectedError,
  );
  assert.throws(
    () => fs.write(fd, createSpoofedBuffer(), offset, length, 0,
                   common.mustNotCall()),
    expectedError,
  );
}
fs.closeSync(fd);

(async () => {
  const handle = await fsPromises.open(file, 'r+');
  try {
    for (const [offset, length] of outOfBoundsRanges) {
      await assert.rejects(
        handle.read(createSpoofedBuffer(), offset, length, 0),
        expectedError,
      );
      await assert.rejects(
        handle.write(createSpoofedBuffer(), offset, length, 0),
        expectedError,
      );
    }
  } finally {
    await handle.close();
  }
})().then(common.mustCall());
