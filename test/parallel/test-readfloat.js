'use strict';
/*
 * Tests to verify we're reading in floats correctly
 */
require('../common');
const assert = require('assert');

/*
 * Test (32 bit) float
 */
function test(clazz) {
  var buffer = new clazz(4);

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0x3f;
  assert.equal(4.600602988224807e-41, buffer.readFloatBE(0));
  assert.equal(1, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0xc0;
  assert.equal(2.6904930515036488e-43, buffer.readFloatBE(0));
  assert.equal(-2, buffer.readFloatLE(0));

  buffer[0] = 0xff;
  buffer[1] = 0xff;
  buffer[2] = 0x7f;
  buffer[3] = 0x7f;
  assert.ok(isNaN(buffer.readFloatBE(0)));
  assert.equal(3.4028234663852886e+38, buffer.readFloatLE(0));

  buffer[0] = 0xab;
  buffer[1] = 0xaa;
  buffer[2] = 0xaa;
  buffer[3] = 0x3e;
  assert.equal(-1.2126478207002966e-12, buffer.readFloatBE(0));
  assert.equal(0.3333333432674408, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0;
  buffer[3] = 0;
  assert.equal(0, buffer.readFloatBE(0));
  assert.equal(0, buffer.readFloatLE(0));
  assert.equal(false, 1 / buffer.readFloatLE(0) < 0);

  buffer[3] = 0x80;
  assert.equal(1.793662034335766e-43, buffer.readFloatBE(0));
  assert.equal(0, buffer.readFloatLE(0));
  assert.equal(true, 1 / buffer.readFloatLE(0) < 0);

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0x7f;
  assert.equal(4.609571298396486e-41, buffer.readFloatBE(0));
  assert.equal(Infinity, buffer.readFloatLE(0));

  buffer[0] = 0;
  buffer[1] = 0;
  buffer[2] = 0x80;
  buffer[3] = 0xff;
  assert.equal(4.627507918739843e-41, buffer.readFloatBE(0));
  assert.equal(-Infinity, buffer.readFloatLE(0));
}


test(Buffer);
