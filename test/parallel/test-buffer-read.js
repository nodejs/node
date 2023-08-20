'use strict';
require('../common');
const assert = require('assert');

// Testing basic buffer read functions
const buf = Buffer.from([0xa4, 0xfd, 0x48, 0xea, 0xcf, 0xff, 0xd9, 0x01, 0xde]);

function read(buff, funx, args, expected) {
  assert.strictEqual(buff[funx](...args), expected);
  assert.throws(
    () => buff[funx](-1, args[1]),
    { code: 'ERR_OUT_OF_RANGE' }
  );
}

// Testing basic functionality of readDoubleBE() and readDoubleLE()
read(buf, 'readDoubleBE', [1], -3.1827727774563287e+295);
read(buf, 'readDoubleLE', [1], -6.966010051009108e+144);

// Testing basic functionality of readFloatBE() and readFloatLE()
read(buf, 'readFloatBE', [1], -1.6691549692541768e+37);
read(buf, 'readFloatLE', [1], -7861303808);

// Testing basic functionality of readInt8()
read(buf, 'readInt8', [1], -3);

// Testing basic functionality of readInt16BE() and readInt16LE()
read(buf, 'readInt16BE', [1], -696);
read(buf, 'readInt16LE', [1], 0x48fd);

// Testing basic functionality of readInt32BE() and readInt32LE()
read(buf, 'readInt32BE', [1], -45552945);
read(buf, 'readInt32LE', [1], -806729475);

// Testing basic functionality of readIntBE() and readIntLE()
read(buf, 'readIntBE', [1, 1], -3);
read(buf, 'readIntLE', [2, 1], 0x48);

// Testing basic functionality of readUInt8()
read(buf, 'readUInt8', [1], 0xfd);

// Testing basic functionality of readUInt16BE() and readUInt16LE()
read(buf, 'readUInt16BE', [2], 0x48ea);
read(buf, 'readUInt16LE', [2], 0xea48);

// Testing basic functionality of readUInt32BE() and readUInt32LE()
read(buf, 'readUInt32BE', [1], 0xfd48eacf);
read(buf, 'readUInt32LE', [1], 0xcfea48fd);

// Testing basic functionality of readUIntBE() and readUIntLE()
read(buf, 'readUIntBE', [2, 2], 0x48ea);
read(buf, 'readUIntLE', [2, 2], 0xea48);

// Error name and message
const OOR_ERROR =
{
  name: 'RangeError'
};

const OOB_ERROR =
{
  name: 'RangeError',
  message: 'Attempt to access memory outside buffer bounds'
};

// Attempt to overflow buffers, similar to previous bug in array buffers
assert.throws(
  () => Buffer.allocUnsafe(8).readFloatBE(0xffffffff), OOR_ERROR);

assert.throws(
  () => Buffer.allocUnsafe(8).readFloatLE(0xffffffff), OOR_ERROR);

// Ensure negative values can't get past offset
assert.throws(
  () => Buffer.allocUnsafe(8).readFloatBE(-1), OOR_ERROR);
assert.throws(
  () => Buffer.allocUnsafe(8).readFloatLE(-1), OOR_ERROR);

// Offset checks
{
  const buf = Buffer.allocUnsafe(0);

  assert.throws(
    () => buf.readUInt8(0), OOB_ERROR);
  assert.throws(
    () => buf.readInt8(0), OOB_ERROR);
}

[16, 32].forEach((bit) => {
  const buf = Buffer.allocUnsafe(bit / 8 - 1);
  [`Int${bit}B`, `Int${bit}L`, `UInt${bit}B`, `UInt${bit}L`].forEach((fn) => {
    assert.throws(
      () => buf[`read${fn}E`](0), OOB_ERROR);
  });
});

[16, 32].forEach((bits) => {
  const buf = Buffer.from([0xFF, 0xFF, 0xFF, 0xFF]);
  ['LE', 'BE'].forEach((endian) => {
    assert.strictEqual(buf[`readUInt${bits}${endian}`](0),
                       (0xFFFFFFFF >>> (32 - bits)));

    assert.strictEqual(buf[`readInt${bits}${endian}`](0),
                       (0xFFFFFFFF >> (32 - bits)));
  });
});
