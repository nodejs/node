'use strict';

// Tests to verify doubles are correctly written

require('../common');
const assert = require('assert');

const buffer = Buffer.allocUnsafe(16);

buffer.writeDoubleBE(2.225073858507201e-308, 0);
buffer.writeDoubleLE(2.225073858507201e-308, 8);
assert.ok(buffer.equals(new Uint8Array([
  0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x00
])));

buffer.writeDoubleBE(1.0000000000000004, 0);
buffer.writeDoubleLE(1.0000000000000004, 8);
assert.ok(buffer.equals(new Uint8Array([
  0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f
])));

buffer.writeDoubleBE(-2, 0);
buffer.writeDoubleLE(-2, 8);
assert.ok(buffer.equals(new Uint8Array([
  0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0
])));

buffer.writeDoubleBE(1.7976931348623157e+308, 0);
buffer.writeDoubleLE(1.7976931348623157e+308, 8);
assert.ok(buffer.equals(new Uint8Array([
  0x7f, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0x7f
])));

buffer.writeDoubleBE(0 * -1, 0);
buffer.writeDoubleLE(0 * -1, 8);
assert.ok(buffer.equals(new Uint8Array([
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80
])));

buffer.writeDoubleBE(Infinity, 0);
buffer.writeDoubleLE(Infinity, 8);

assert.ok(buffer.equals(new Uint8Array([
  0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x7F
])));

assert.strictEqual(buffer.readDoubleBE(0), Infinity);
assert.strictEqual(buffer.readDoubleLE(8), Infinity);

buffer.writeDoubleBE(-Infinity, 0);
buffer.writeDoubleLE(-Infinity, 8);

assert.ok(buffer.equals(new Uint8Array([
  0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xFF
])));

assert.strictEqual(buffer.readDoubleBE(0), -Infinity);
assert.strictEqual(buffer.readDoubleLE(8), -Infinity);

buffer.writeDoubleBE(NaN, 0);
buffer.writeDoubleLE(NaN, 8);

// JS only knows a single NaN but there exist two platform specific
// implementations. Therefore, allow both quiet and signalling NaNs.
if (buffer[1] === 0xF7) {
  assert.ok(buffer.equals(new Uint8Array([
    0x7F, 0xF7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x7F
  ])));
} else {
  assert.ok(buffer.equals(new Uint8Array([
    0x7F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x7F
  ])));
}

assert.ok(Number.isNaN(buffer.readDoubleBE(0)));
assert.ok(Number.isNaN(buffer.readDoubleLE(8)));

// OOB in writeDouble{LE,BE} should throw.
{
  const small = Buffer.allocUnsafe(1);

  ['writeDoubleLE', 'writeDoubleBE'].forEach((fn) => {

    // Verify that default offset works fine.
    buffer[fn](23, undefined);
    buffer[fn](23);

    assert.throws(
      () => small[fn](11.11, 0),
      {
        code: 'ERR_BUFFER_OUT_OF_BOUNDS',
        name: 'RangeError [ERR_BUFFER_OUT_OF_BOUNDS]',
        message: 'Attempt to write outside buffer bounds'
      });

    ['', '0', null, {}, [], () => {}, true, false].forEach((off) => {
      assert.throws(
        () => small[fn](23, off),
        { code: 'ERR_INVALID_ARG_TYPE' });
    });

    [Infinity, -1, 9].forEach((offset) => {
      assert.throws(
        () => buffer[fn](23, offset),
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError [ERR_OUT_OF_RANGE]',
          message: 'The value of "offset" is out of range. ' +
                     `It must be >= 0 and <= 8. Received ${offset}`
        });
    });

    [NaN, 1.01].forEach((offset) => {
      assert.throws(
        () => buffer[fn](42, offset),
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError [ERR_OUT_OF_RANGE]',
          message: 'The value of "offset" is out of range. ' +
                   `It must be an integer. Received ${offset}`
        });
    });
  });
}
