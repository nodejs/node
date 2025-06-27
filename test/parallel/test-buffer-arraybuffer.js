'use strict';

require('../common');
const assert = require('assert');

const LENGTH = 16;

const ab = new ArrayBuffer(LENGTH);
const dv = new DataView(ab);
const ui = new Uint8Array(ab);
const buf = Buffer.from(ab);


assert.ok(buf instanceof Buffer);
assert.strictEqual(buf.parent, buf.buffer);
assert.strictEqual(buf.buffer, ab);
assert.strictEqual(buf.length, ab.byteLength);


buf.fill(0xC);
for (let i = 0; i < LENGTH; i++) {
  assert.strictEqual(ui[i], 0xC);
  ui[i] = 0xF;
  assert.strictEqual(buf[i], 0xF);
}

buf.writeUInt32LE(0xF00, 0);
buf.writeUInt32BE(0xB47, 4);
buf.writeDoubleLE(3.1415, 8);

assert.strictEqual(dv.getUint32(0, true), 0xF00);
assert.strictEqual(dv.getUint32(4), 0xB47);
assert.strictEqual(dv.getFloat64(8, true), 3.1415);


// Now test protecting users from doing stupid things

assert.throws(function() {
  function AB() { }
  Object.setPrototypeOf(AB, ArrayBuffer);
  Object.setPrototypeOf(AB.prototype, ArrayBuffer.prototype);
  Buffer.from(new AB());
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
  message: 'The first argument must be of type string or an instance of ' +
           'Buffer, ArrayBuffer, or Array or an Array-like Object. Received ' +
           'an instance of AB'
});

// Test the byteOffset and length arguments
{
  const ab = new Uint8Array(5);
  ab[0] = 1;
  ab[1] = 2;
  ab[2] = 3;
  ab[3] = 4;
  ab[4] = 5;
  const buf = Buffer.from(ab.buffer, 1, 3);
  assert.strictEqual(buf.length, 3);
  assert.strictEqual(buf[0], 2);
  assert.strictEqual(buf[1], 3);
  assert.strictEqual(buf[2], 4);
  buf[0] = 9;
  assert.strictEqual(ab[1], 9);

  assert.throws(() => Buffer.from(ab.buffer, 6), {
    code: 'ERR_BUFFER_OUT_OF_BOUNDS',
    name: 'RangeError',
    message: '"offset" is outside of buffer bounds'
  });
  assert.throws(() => Buffer.from(ab.buffer, 3, 6), {
    code: 'ERR_BUFFER_OUT_OF_BOUNDS',
    name: 'RangeError',
    message: '"length" is outside of buffer bounds'
  });
}

// Test the deprecated Buffer() version also
{
  const ab = new Uint8Array(5);
  ab[0] = 1;
  ab[1] = 2;
  ab[2] = 3;
  ab[3] = 4;
  ab[4] = 5;
  const buf = Buffer(ab.buffer, 1, 3);
  assert.strictEqual(buf.length, 3);
  assert.strictEqual(buf[0], 2);
  assert.strictEqual(buf[1], 3);
  assert.strictEqual(buf[2], 4);
  buf[0] = 9;
  assert.strictEqual(ab[1], 9);

  assert.throws(() => Buffer(ab.buffer, 6), {
    code: 'ERR_BUFFER_OUT_OF_BOUNDS',
    name: 'RangeError',
    message: '"offset" is outside of buffer bounds'
  });
  assert.throws(() => Buffer(ab.buffer, 3, 6), {
    code: 'ERR_BUFFER_OUT_OF_BOUNDS',
    name: 'RangeError',
    message: '"length" is outside of buffer bounds'
  });
}

{
  // If byteOffset is not numeric, it defaults to 0.
  const ab = new ArrayBuffer(10);
  const expected = Buffer.from(ab, 0);
  assert.deepStrictEqual(Buffer.from(ab, 'fhqwhgads'), expected);
  assert.deepStrictEqual(Buffer.from(ab, NaN), expected);
  assert.deepStrictEqual(Buffer.from(ab, {}), expected);
  assert.deepStrictEqual(Buffer.from(ab, []), expected);

  // If byteOffset can be converted to a number, it will be.
  assert.deepStrictEqual(Buffer.from(ab, [1]), Buffer.from(ab, 1));

  // If byteOffset is Infinity, throw.
  assert.throws(() => {
    Buffer.from(ab, Infinity);
  }, {
    code: 'ERR_BUFFER_OUT_OF_BOUNDS',
    name: 'RangeError',
    message: '"offset" is outside of buffer bounds'
  });
}

{
  // If length is not numeric, it defaults to 0.
  const ab = new ArrayBuffer(10);
  const expected = Buffer.from(ab, 0, 0);
  assert.deepStrictEqual(Buffer.from(ab, 0, 'fhqwhgads'), expected);
  assert.deepStrictEqual(Buffer.from(ab, 0, NaN), expected);
  assert.deepStrictEqual(Buffer.from(ab, 0, {}), expected);
  assert.deepStrictEqual(Buffer.from(ab, 0, []), expected);

  // If length can be converted to a number, it will be.
  assert.deepStrictEqual(Buffer.from(ab, 0, [1]), Buffer.from(ab, 0, 1));

  // If length is Infinity, throw.
  assert.throws(() => {
    Buffer.from(ab, 0, Infinity);
  }, {
    code: 'ERR_BUFFER_OUT_OF_BOUNDS',
    name: 'RangeError',
    message: '"length" is outside of buffer bounds'
  });
}

// Test an array like entry with the length set to NaN.
assert.deepStrictEqual(Buffer.from({ length: NaN }), Buffer.alloc(0));
