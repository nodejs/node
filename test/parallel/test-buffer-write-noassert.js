'use strict';
require('../common');
const assert = require('assert');

// testing buffer write functions

function write(funx, args, result, res) {
  {
    const buf = Buffer.alloc(9);
    assert.strictEqual(buf[funx](...args), result);
    assert.deepStrictEqual(buf, res);
  }

  {
    const invalidArgs = Array.from(args);
    invalidArgs[1] = -1;
    assert.throws(
      () => Buffer.alloc(9)[funx](...invalidArgs),
      /^RangeError: (?:Index )?out of range(?: index)?$/
    );
  }

  {
    const error = /Int/.test(funx) ?
      /^TypeError: "buffer" argument must be a Buffer or Uint8Array$/ :
      /^TypeError: argument should be a Buffer$/;

    assert.throws(
      () => Buffer.alloc(9)[funx].apply(new Uint32Array(1), args),
      error
    );
  }

  {
    const buf2 = Buffer.alloc(9);
    assert.strictEqual(buf2[funx](...args, true), result);
    assert.deepStrictEqual(buf2, res);
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
