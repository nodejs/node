'use strict';

require('../common');

// This test ensures that fs.writeSync accepts "named parameters" object
// and doesn't interpret objects as strings

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dest = path.resolve(tmpdir.path, 'tmp.txt');
const buffer = Buffer.from('zyx');

function testInvalid(dest, expectedCode, ...bufferAndOptions) {
  let fd;
  try {
    fd = fs.openSync(dest, 'w+');
    assert.throws(
      () => fs.writeSync(fd, ...bufferAndOptions),
      { code: expectedCode });
  } finally {
    if (fd != null) fs.closeSync(fd);
  }
}

function testValid(dest, buffer, options) {
  let fd;
  try {
    fd = fs.openSync(dest, 'w+');
    const bytesWritten = fs.writeSync(fd, buffer, options);
    const bytesRead = fs.readSync(fd, buffer, options);

    assert.ok(bytesWritten >= bytesRead);
    if (options.length !== undefined && options.length !== null) {
      assert.strictEqual(bytesWritten, options.length);
    }
  } finally {
    if (fd != null) fs.closeSync(fd);
  }
}

{
  // Test if second argument is not wrongly interpreted as string or options
  for (const badBuffer of [
    undefined, null, true, 42, 42n, Symbol('42'), NaN, [], () => {},
    {},
    { buffer: 'amNotParam' },
    { string: 'amNotParam' },
    { buffer: new Uint8Array(1) },
    { buffer: new Uint8Array(1).buffer },
    Promise.resolve(new Uint8Array(1)),
    new Date(),
    new String('notPrimitive'),
    { toString() { return 'amObject'; } },
    { [Symbol.toPrimitive]: (hint) => 'amObject' },
  ]) {
    testInvalid(dest, 'ERR_INVALID_ARG_TYPE', badBuffer);
  }

  // First argument (buffer or string) is mandatory
  testInvalid(dest, 'ERR_INVALID_ARG_TYPE');

  // Various invalid options
  testInvalid(dest, 'ERR_OUT_OF_RANGE', buffer, { length: 5 });
  testInvalid(dest, 'ERR_OUT_OF_RANGE', buffer, { offset: 5 });
  testInvalid(dest, 'ERR_OUT_OF_RANGE', buffer, { length: 1, offset: 3 });
  testInvalid(dest, 'ERR_OUT_OF_RANGE', buffer, { length: -1 });
  testInvalid(dest, 'ERR_OUT_OF_RANGE', buffer, { offset: -1 });
  testInvalid(dest, 'ERR_INVALID_ARG_TYPE', buffer, { offset: false });
  testInvalid(dest, 'ERR_INVALID_ARG_TYPE', buffer, { offset: true });

  // Test compatibility with fs.readSync counterpart with reused options
  for (const options of [
    {},
    { length: 1 },
    { position: 5 },
    { length: 1, position: 5 },
    { length: 1, position: -1, offset: 2 },
    { length: null },
    { position: null },
    { offset: 1 },
  ]) {
    testValid(dest, buffer, options);
  }
}
