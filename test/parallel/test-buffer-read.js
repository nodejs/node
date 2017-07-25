'use strict';
require('../common');
const assert = require('assert');

// testing basic buffer read functions
const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea, 0xcf, 0xff, 0xd9, 0x01, 0xde]);

function read(buff, funx, args, expected) {

  assert.strictEqual(buff[funx](...args), expected);
  assert.throws(
    () => buff[funx](-1),
    /^RangeError: Index out of range$/
  );

  assert.doesNotThrow(
    () => assert.strictEqual(buff[funx](...args, true), expected),
    'noAssert does not change return value for valid ranges'
  );

}

// testing basic functionality of readDoubleBE() and readDOubleLE()
read(buf, 'readDoubleBE', [1], -3.1827727774563287e+295);
read(buf, 'readDoubleLE', [1], -6.966010051009108e+144);

// testing basic functionality of readFLoatBE() and readFloatLE()
read(buf, 'readFloatBE', [1], -1.6691549692541768e+37);
read(buf, 'readFloatLE', [1], -7861303808);

// testing basic functionality of readInt8()
read(buf, 'readInt8', [1], -3);

// testing basic functionality of readInt16BE() and readInt16LE()
read(buf, 'readInt16BE', [1], -696);
read(buf, 'readInt16LE', [1], 0x48fd);

// testing basic functionality of readInt32BE() and readInt32LE()
read(buf, 'readInt32BE', [1], -45552945);
read(buf, 'readInt32LE', [1], -806729475);

// testing basic functionality of readIntBE() and readIntLE()
read(buf, 'readIntBE', [1, 1], -3);
read(buf, 'readIntLE', [2, 1], 0x48);

// testing basic functionality of readUInt8()
read(buf, 'readUInt8', [1], 0xfd);

// testing basic functionality of readUInt16BE() and readUInt16LE()
read(buf, 'readUInt16BE', [2], 0x48ea);
read(buf, 'readUInt16LE', [2], 0xea48);

// testing basic functionality of readUInt32BE() and readUInt32LE()
read(buf, 'readUInt32BE', [1], 0xfd48eacf);
read(buf, 'readUInt32LE', [1], 0xcfea48fd);

// testing basic functionality of readUIntBE() and readUIntLE()
read(buf, 'readUIntBE', [2, 0], 0xfd);
read(buf, 'readUIntLE', [2, 0], 0x48);

// attempt to overflow buffers, similar to previous bug in array buffers
assert.throws(() => Buffer.allocUnsafe(8).readFloatLE(0xffffffff),
              RangeError);
assert.throws(() => Buffer.allocUnsafe(8).readFloatLE(0xffffffff),
              RangeError);

// ensure negative values can't get past offset
assert.throws(() => Buffer.allocUnsafe(8).readFloatLE(-1), RangeError);
assert.throws(() => Buffer.allocUnsafe(8).readFloatLE(-1), RangeError);

// offset checks
{
  const buf = Buffer.allocUnsafe(0);

  assert.throws(() => buf.readUInt8(0), RangeError);
  assert.throws(() => buf.readInt8(0), RangeError);
}

{
  const buf = Buffer.from([0xFF]);

  assert.strictEqual(buf.readUInt8(0), 255);
  assert.strictEqual(buf.readInt8(0), -1);
}

[16, 32].forEach((bits) => {
  const buf = Buffer.allocUnsafe(bits / 8 - 1);

  assert.throws(() => buf[`readUInt${bits}BE`](0),
                RangeError,
                `readUInt${bits}BE()`);

  assert.throws(() => buf[`readUInt${bits}LE`](0),
                RangeError,
                `readUInt${bits}LE()`);

  assert.throws(() => buf[`readInt${bits}BE`](0),
                RangeError,
                `readInt${bits}BE()`);

  assert.throws(() => buf[`readInt${bits}LE`](0),
                RangeError,
                `readInt${bits}LE()`);
});

[16, 32].forEach((bits) => {
  const buf = Buffer.from([0xFF, 0xFF, 0xFF, 0xFF]);

  assert.strictEqual(buf[`readUInt${bits}BE`](0),
                     (0xFFFFFFFF >>> (32 - bits)));

  assert.strictEqual(buf[`readUInt${bits}LE`](0),
                     (0xFFFFFFFF >>> (32 - bits)));

  assert.strictEqual(buf[`readInt${bits}BE`](0),
                     (0xFFFFFFFF >> (32 - bits)));

  assert.strictEqual(buf[`readInt${bits}LE`](0),
                     (0xFFFFFFFF >> (32 - bits)));
});

// test for common read(U)IntLE/BE
{
  const buf = Buffer.from([0x01, 0x02, 0x03, 0x04, 0x05, 0x06]);

  assert.strictEqual(buf.readUIntLE(0, 1), 0x01);
  assert.strictEqual(buf.readUIntBE(0, 1), 0x01);
  assert.strictEqual(buf.readUIntLE(0, 3), 0x030201);
  assert.strictEqual(buf.readUIntBE(0, 3), 0x010203);
  assert.strictEqual(buf.readUIntLE(0, 5), 0x0504030201);
  assert.strictEqual(buf.readUIntBE(0, 5), 0x0102030405);
  assert.strictEqual(buf.readUIntLE(0, 6), 0x060504030201);
  assert.strictEqual(buf.readUIntBE(0, 6), 0x010203040506);
  assert.strictEqual(buf.readIntLE(0, 1), 0x01);
  assert.strictEqual(buf.readIntBE(0, 1), 0x01);
  assert.strictEqual(buf.readIntLE(0, 3), 0x030201);
  assert.strictEqual(buf.readIntBE(0, 3), 0x010203);
  assert.strictEqual(buf.readIntLE(0, 5), 0x0504030201);
  assert.strictEqual(buf.readIntBE(0, 5), 0x0102030405);
  assert.strictEqual(buf.readIntLE(0, 6), 0x060504030201);
  assert.strictEqual(buf.readIntBE(0, 6), 0x010203040506);
}
