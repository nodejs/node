'use strict';

require('../common');
const assert = require('assert');

// Test 32 bit float
const buffer = Buffer.alloc(4);

buffer[0] = 0;
buffer[1] = 0;
buffer[2] = 0x80;
buffer[3] = 0x3f;
assert.strictEqual(buffer.readFloatBE(0), 4.600602988224807e-41);
assert.strictEqual(buffer.readFloatLE(0), 1);

buffer[0] = 0;
buffer[1] = 0;
buffer[2] = 0;
buffer[3] = 0xc0;
assert.strictEqual(buffer.readFloatBE(0), 2.6904930515036488e-43);
assert.strictEqual(buffer.readFloatLE(0), -2);

buffer[0] = 0xff;
buffer[1] = 0xff;
buffer[2] = 0x7f;
buffer[3] = 0x7f;
assert.ok(Number.isNaN(buffer.readFloatBE(0)));
assert.strictEqual(buffer.readFloatLE(0), 3.4028234663852886e+38);

buffer[0] = 0xab;
buffer[1] = 0xaa;
buffer[2] = 0xaa;
buffer[3] = 0x3e;
assert.strictEqual(buffer.readFloatBE(0), -1.2126478207002966e-12);
assert.strictEqual(buffer.readFloatLE(0), 0.3333333432674408);

buffer[0] = 0;
buffer[1] = 0;
buffer[2] = 0;
buffer[3] = 0;
assert.strictEqual(buffer.readFloatBE(0), 0);
assert.strictEqual(buffer.readFloatLE(0), 0);
assert.ok(1 / buffer.readFloatLE(0) >= 0);

buffer[3] = 0x80;
assert.strictEqual(buffer.readFloatBE(0), 1.793662034335766e-43);
assert.strictEqual(buffer.readFloatLE(0), -0);
assert.ok(1 / buffer.readFloatLE(0) < 0);

buffer[0] = 0;
buffer[1] = 0;
buffer[2] = 0x80;
buffer[3] = 0x7f;
assert.strictEqual(buffer.readFloatBE(0), 4.609571298396486e-41);
assert.strictEqual(buffer.readFloatLE(0), Infinity);

buffer[0] = 0;
buffer[1] = 0;
buffer[2] = 0x80;
buffer[3] = 0xff;
assert.strictEqual(buffer.readFloatBE(0), 4.627507918739843e-41);
assert.strictEqual(buffer.readFloatLE(0), -Infinity);

['readFloatLE', 'readFloatBE'].forEach((fn) => {

  // Verify that default offset works fine.
  buffer[fn](undefined);
  buffer[fn]();

  ['', '0', null, {}, [], () => {}, true, false].forEach((off) => {
    assert.throws(
      () => buffer[fn](off),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });

  [Infinity, -1, 1].forEach((offset) => {
    assert.throws(
      () => buffer[fn](offset),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
        message: 'The value of "offset" is out of range. ' +
                 `It must be >= 0 and <= 0. Received ${offset}`
      });
  });

  assert.throws(
    () => Buffer.alloc(1)[fn](1),
    {
      code: 'ERR_BUFFER_OUT_OF_BOUNDS',
      name: 'RangeError',
      message: 'Attempt to access memory outside buffer bounds'
    });

  [NaN, 1.01].forEach((offset) => {
    assert.throws(
      () => buffer[fn](offset),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
        message: 'The value of "offset" is out of range. ' +
                 `It must be an integer. Received ${offset}`
      });
  });
});
