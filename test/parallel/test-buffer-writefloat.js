'use strict';

// Tests to verify floats are correctly written

require('../common');
const assert = require('assert');

const buffer = Buffer.allocUnsafe(8);

buffer.writeFloatBE(1, 0);
buffer.writeFloatLE(1, 4);
assert.ok(buffer.equals(
  new Uint8Array([ 0x3f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3f ])));

buffer.writeFloatBE(1 / 3, 0);
buffer.writeFloatLE(1 / 3, 4);
assert.ok(buffer.equals(
  new Uint8Array([ 0x3e, 0xaa, 0xaa, 0xab, 0xab, 0xaa, 0xaa, 0x3e ])));

buffer.writeFloatBE(3.4028234663852886e+38, 0);
buffer.writeFloatLE(3.4028234663852886e+38, 4);
assert.ok(buffer.equals(
  new Uint8Array([ 0x7f, 0x7f, 0xff, 0xff, 0xff, 0xff, 0x7f, 0x7f ])));

buffer.writeFloatLE(1.1754943508222875e-38, 0);
buffer.writeFloatBE(1.1754943508222875e-38, 4);
assert.ok(buffer.equals(
  new Uint8Array([ 0x00, 0x00, 0x80, 0x00, 0x00, 0x80, 0x00, 0x00 ])));

buffer.writeFloatBE(0 * -1, 0);
buffer.writeFloatLE(0 * -1, 4);
assert.ok(buffer.equals(
  new Uint8Array([ 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80 ])));

buffer.writeFloatBE(Infinity, 0);
buffer.writeFloatLE(Infinity, 4);
assert.ok(buffer.equals(
  new Uint8Array([ 0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0x7F ])));

assert.strictEqual(buffer.readFloatBE(0), Infinity);
assert.strictEqual(buffer.readFloatLE(4), Infinity);

buffer.writeFloatBE(-Infinity, 0);
buffer.writeFloatLE(-Infinity, 4);
assert.ok(buffer.equals(
  new Uint8Array([ 0xFF, 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF ])));

assert.strictEqual(buffer.readFloatBE(0), -Infinity);
assert.strictEqual(buffer.readFloatLE(4), -Infinity);

buffer.writeFloatBE(NaN, 0);
buffer.writeFloatLE(NaN, 4);

// JS only knows a single NaN but there exist two platform specific
// implementations. Therefore, allow both quiet and signalling NaNs.
if (buffer[1] === 0xBF) {
  assert.ok(
    buffer.equals(new Uint8Array(
      [ 0x7F, 0xBF, 0xFF, 0xFF, 0xFF, 0xFF, 0xBF, 0x7F ])));
} else {
  assert.ok(
    buffer.equals(new Uint8Array(
      [ 0x7F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x7F ])));
}

assert.ok(Number.isNaN(buffer.readFloatBE(0)));
assert.ok(Number.isNaN(buffer.readFloatLE(4)));

// OOB in writeFloat{LE,BE} should throw.
{
  const small = Buffer.allocUnsafe(1);

  ['writeFloatLE', 'writeFloatBE'].forEach((fn) => {

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
        { code: 'ERR_INVALID_ARG_TYPE' }
      );
    });

    [Infinity, -1, 5].forEach((offset) => {
      assert.throws(
        () => buffer[fn](23, offset),
        {
          code: 'ERR_OUT_OF_RANGE',
          name: 'RangeError [ERR_OUT_OF_RANGE]',
          message: 'The value of "offset" is out of range. ' +
                   `It must be >= 0 and <= 4. Received ${offset}`
        }
      );
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
