'use strict';

require('../common');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const assert = require('assert');
const filepath = fixtures.path('x.txt');
const fd = fs.openSync(filepath, 'r');

const expected = Buffer.from('xyz\n');
const defaultBuffer1 = Buffer.allocUnsafe(expected.length);
const defaultBuffer2 = Buffer.allocUnsafe(expected.length);
const defaultBuffer3 = Buffer.allocUnsafe(expected.length);

// Test passing in an empty options object
const result1 = fs.readSync(fd, defaultBuffer1, { position: 0 });
assert.strictEqual(result1, expected.length);
assert.deepStrictEqual(defaultBuffer1, expected);

// Test not passing in any options object
const result2 = fs.readSync(fd, defaultBuffer2);
assert.strictEqual(result2, expected.length);
assert.deepStrictEqual(defaultBuffer2, expected);


// Test passing in options
const result3 = fs.readSync(fd, defaultBuffer3, {
  offset: 0,
  length: defaultBuffer3.length,
  position: 0
});

assert.strictEqual(result3, expected.length);
assert.deepStrictEqual(defaultBuffer3, expected);
