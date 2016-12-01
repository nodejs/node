'use strict';

require('../common');
const assert = require('assert');

const Buffer = require('buffer').Buffer;
const LENGTH = 16;

const ab = new ArrayBuffer(LENGTH);
const dv = new DataView(ab);
const ui = new Uint8Array(ab);
const buf = new Buffer(ab);


assert.ok(buf instanceof Buffer);
// For backwards compatibility of old .parent property test that if buf is not
// a slice then .parent should be undefined.
assert.strictEqual(buf.parent, undefined);
assert.strictEqual(buf.buffer, ab);
assert.strictEqual(buf.length, ab.byteLength);


buf.fill(0xC);
for (let i = 0; i < LENGTH; i++) {
  assert.equal(ui[i], 0xC);
  ui[i] = 0xF;
  assert.equal(buf[i], 0xF);
}

buf.writeUInt32LE(0xF00, 0);
buf.writeUInt32BE(0xB47, 4);
buf.writeDoubleLE(3.1415, 8);

assert.equal(dv.getUint32(0, true), 0xF00);
assert.equal(dv.getUint32(4), 0xB47);
assert.equal(dv.getFloat64(8, true), 3.1415);


// Now test protecting users from doing stupid things

assert.throws(function() {
  function AB() { }
  Object.setPrototypeOf(AB, ArrayBuffer);
  Object.setPrototypeOf(AB.prototype, ArrayBuffer.prototype);
  new Buffer(new AB());
}, TypeError);

// write{Double,Float}{LE,BE} with noAssert should not crash, cf. #3766
const b = Buffer.allocUnsafe(1);
b.writeFloatLE(11.11, 0, true);
b.writeFloatBE(11.11, 0, true);
b.writeDoubleLE(11.11, 0, true);
b.writeDoubleBE(11.11, 0, true);
