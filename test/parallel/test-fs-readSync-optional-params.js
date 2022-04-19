'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');
const filepath = fixtures.path('x.txt');

const expected = Buffer.from('xyz\n');

function runTest(defaultBuffer, options) {
  let fd;
  try {
    fd = fs.openSync(filepath, 'r');
    const result = fs.readSync(fd, defaultBuffer, options);
    assert.strictEqual(result, expected.length);
    assert.deepStrictEqual(defaultBuffer, expected);
  } finally {
    if (fd != null) fs.closeSync(fd);
  }
}

for (const options of [

  // Test options object
  { offset: 0 },
  { length: expected.length },
  { position: 0 },
  { offset: 0, length: expected.length },
  { offset: 0, position: 0 },
  { length: expected.length, position: 0 },
  { offset: 0, length: expected.length, position: 0 },

  { offset: null },
  { position: null },
  { position: -1 },
  { position: 0n },

  // Test default params
  {},
  null,
  undefined,

  // Test if bad params are interpreted as default (not mandatory)
  false,
  true,
  Infinity,
  42n,
  Symbol(),

  // Test even more malicious corner cases
  '4'.repeat(expected.length),
  new String('4444'),
  [4, 4, 4, 4],
]) {
  runTest(Buffer.allocUnsafe(expected.length), options);
}
