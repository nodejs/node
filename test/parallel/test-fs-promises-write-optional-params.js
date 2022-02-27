'use strict';

const common = require('../common');

// This test ensures that filehandle.write accepts "named parameters" object
// and doesn't interpret objects as strings

const assert = require('assert');
const fsPromises = require('fs').promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dest = path.resolve(tmpdir.path, 'tmp.txt');
const buffer = Buffer.from('zyx');

async function testInvalid(dest, expectedCode, params) {
  let fh;
  try {
    fh = await fsPromises.open(dest, 'w+');
    await assert.rejects(
      async () => fh.write(params),
      { code: expectedCode });
  } finally {
    await fh?.close();
  }
}

async function testValid(dest, params) {
  let fh;
  try {
    fh = await fsPromises.open(dest, 'w+');
    const writeResult = await fh.write(params);
    const writeBufCopy = Uint8Array.prototype.slice.call(writeResult.buffer);
    const readResult = await fh.read(params);
    const readBufCopy = Uint8Array.prototype.slice.call(readResult.buffer);

    assert.ok(writeResult.bytesWritten >= readResult.bytesRead);
    if (params.length !== undefined && params.length !== null) {
      assert.strictEqual(writeResult.bytesWritten, params.length);
    }
    if (params.offset === undefined || params.offset === 0) {
      assert.deepStrictEqual(writeBufCopy, readBufCopy);
    }
    assert.deepStrictEqual(writeResult.buffer, readResult.buffer);
  } finally {
    await fh?.close();
  }
}

(async () => {
  // Test if first argument is not wrongly interpreted as ArrayBufferView|string
  for (const badParams of [
    undefined, null, true, 42, 42n, Symbol('42'), NaN, [],
    {},
    { buffer: 'amNotParam' },
    { string: 'amNotParam' },
    { buffer: new Uint8Array(1).buffer },
    new Date(),
    new String('notPrimitive'),
    { toString() { return 'amObject'; } },
    { [Symbol.toPrimitive]: (hint) => 'amObject' },
  ]) {
    await testInvalid(dest, 'ERR_INVALID_ARG_TYPE', badParams);
  }

  // Various invalid params
  await testInvalid(dest, 'ERR_OUT_OF_RANGE', { buffer, length: 5 });
  await testInvalid(dest, 'ERR_OUT_OF_RANGE', { buffer, offset: 5 });
  await testInvalid(dest, 'ERR_OUT_OF_RANGE', { buffer, length: 1, offset: 3 });
  await testInvalid(dest, 'ERR_OUT_OF_RANGE', { buffer, length: -1 });
  await testInvalid(dest, 'ERR_OUT_OF_RANGE', { buffer, offset: -1 });

  // Test compatibility with filehandle.read counterpart with reused params
  for (const params of [
    { buffer },
    { buffer, length: 1 },
    { buffer, position: 5 },
    { buffer, length: 1, position: 5 },
    { buffer, length: 1, position: -1, offset: 2 },
    { buffer, length: null },
    { buffer, offset: 1 },
  ]) {
    await testValid(dest, params);
  }
})().then(common.mustCall());
