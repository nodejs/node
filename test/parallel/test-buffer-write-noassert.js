'use strict';
require('../common');
const assert = require('assert');

// testing buffer write functions

const outOfRange = /^RangeError\b.*\bIndex out of range$/;

function write(funx, args, result, res) {
  {
    const buf = Buffer.alloc(9);
    assert.strictEqual(buf[funx](...args), result);
    assert.deepStrictEqual(buf, res);
  }

  writeInvalidOffset(-1);
  writeInvalidOffset(9);

  if (!/Int/.test(funx)) {
    assert.throws(
      () => Buffer.alloc(9)[funx].apply(new Map(), args),
      /^TypeError: argument should be a Buffer$/
    );
  }

  {
    const buf2 = Buffer.alloc(9);
    assert.strictEqual(buf2[funx](...args, true), result);
    assert.deepStrictEqual(buf2, res);
  }

  function writeInvalidOffset(offset) {
    const newArgs = Array.from(args);
    newArgs[1] = offset;
    assert.throws(() => Buffer.alloc(9)[funx](...newArgs), outOfRange);

    const buf = Buffer.alloc(9);
    buf[funx](...newArgs, true);
    assert.deepStrictEqual(buf, Buffer.alloc(9));
  }
}

write('writeInt8', [1, 0], 1, Buffer.from([1, 0, 0, 0, 0, 0, 0, 0, 0]));
write('writeIntBE', [1, 1, 4], 5, Buffer.from([0, 0, 0, 0, 1, 0, 0, 0, 0]));
write('writeIntLE', [1, 1, 4], 5, Buffer.from([0, 1, 0, 0, 0, 0, 0, 0, 0]));
write('writeInt16LE', [1, 1], 3, Buffer.from([0, 1, 0, 0, 0, 0, 0, 0, 0]));
write('writeInt16BE', [1, 1], 3, Buffer.from([0, 0, 1, 0, 0, 0, 0, 0, 0]));
write('writeInt32LE', [1, 1], 5, Buffer.from([0, 1, 0, 0, 0, 0, 0, 0, 0]));
write('writeInt32BE', [1, 1], 5, Buffer.from([0, 0, 0, 0, 1, 0, 0, 0, 0]));
write('writeUInt8', [1, 0], 1, Buffer.from([1, 0, 0, 0, 0, 0, 0, 0, 0]));
write('writeUIntLE', [1, 1, 4], 5, Buffer.from([0, 1, 0, 0, 0, 0, 0, 0, 0]));
write('writeUIntBE', [1, 1, 4], 5, Buffer.from([0, 0, 0, 0, 1, 0, 0, 0, 0]));
write('writeUInt16LE', [1, 1], 3, Buffer.from([0, 1, 0, 0, 0, 0, 0, 0, 0]));
write('writeUInt16BE', [1, 1], 3, Buffer.from([0, 0, 1, 0, 0, 0, 0, 0, 0]));
write('writeUInt32LE', [1, 1], 5, Buffer.from([0, 1, 0, 0, 0, 0, 0, 0, 0]));
write('writeUInt32BE', [1, 1], 5, Buffer.from([0, 0, 0, 0, 1, 0, 0, 0, 0]));
write('writeDoubleBE', [1, 1], 9, Buffer.from([0, 63, 240, 0, 0, 0, 0, 0, 0]));
write('writeDoubleLE', [1, 1], 9, Buffer.from([0, 0, 0, 0, 0, 0, 0, 240, 63]));
write('writeFloatBE', [1, 1], 5, Buffer.from([0, 63, 128, 0, 0, 0, 0, 0, 0]));
write('writeFloatLE', [1, 1], 5, Buffer.from([0, 0, 0, 128, 63, 0, 0, 0, 0]));

function writePartial(funx, args, result, res) {
  assert.throws(() => Buffer.alloc(9)[funx](...args), outOfRange);
  const buf = Buffer.alloc(9);
  assert.strictEqual(buf[funx](...args, true), result);
  assert.deepStrictEqual(buf, res);
}

// Test partial writes (cases where the buffer isn't large enough to hold the
// entire value, but is large enough to hold parts of it).
writePartial('writeIntBE', [0x0eadbeef, 6, 4], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0x0e, 0xad, 0xbe]));
writePartial('writeIntLE', [0x0eadbeef, 6, 4], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0xef, 0xbe, 0xad]));
writePartial('writeInt16BE', [0x1234, 8], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0, 0, 0x12]));
writePartial('writeInt16LE', [0x1234, 8], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0, 0, 0x34]));
writePartial('writeInt32BE', [0x0eadbeef, 6], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0x0e, 0xad, 0xbe]));
writePartial('writeInt32LE', [0x0eadbeef, 6], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0xef, 0xbe, 0xad]));
writePartial('writeUIntBE', [0xdeadbeef, 6, 4], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0xde, 0xad, 0xbe]));
writePartial('writeUIntLE', [0xdeadbeef, 6, 4], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0xef, 0xbe, 0xad]));
writePartial('writeUInt16BE', [0x1234, 8], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0, 0, 0x12]));
writePartial('writeUInt16LE', [0x1234, 8], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0, 0, 0x34]));
writePartial('writeUInt32BE', [0xdeadbeef, 6], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0xde, 0xad, 0xbe]));
writePartial('writeUInt32LE', [0xdeadbeef, 6], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0xef, 0xbe, 0xad]));
writePartial('writeDoubleBE', [1, 2], 10,
             Buffer.from([0, 0, 63, 240, 0, 0, 0, 0, 0]));
writePartial('writeDoubleLE', [1, 2], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0, 0, 240]));
writePartial('writeFloatBE', [1, 6], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 63, 128, 0]));
writePartial('writeFloatLE', [1, 6], 10,
             Buffer.from([0, 0, 0, 0, 0, 0, 0, 0, 128]));
