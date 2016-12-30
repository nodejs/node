'use strict';
/*
 * Tests to verify we're writing signed integers correctly
 */
require('../common');
const assert = require('assert');

function test8(clazz) {
  var buffer = new clazz(2);

  buffer.writeInt8(0x23, 0);
  buffer.writeInt8(-5, 1);

  assert.equal(0x23, buffer[0]);
  assert.equal(0xfb, buffer[1]);

  /* Make sure we handle truncation correctly */
  assert.throws(function() {
    buffer.writeInt8(0xabc, 0);
  });
  assert.throws(function() {
    buffer.writeInt8(0xabc, 0);
  });

  /* Make sure we handle min/max correctly */
  buffer.writeInt8(0x7f, 0);
  buffer.writeInt8(-0x80, 1);

  assert.equal(0x7f, buffer[0]);
  assert.equal(0x80, buffer[1]);
  assert.throws(function() {
    buffer.writeInt8(0x7f + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt8(-0x80 - 1, 0);
  });
}


function test16(clazz) {
  var buffer = new clazz(6);

  buffer.writeInt16BE(0x0023, 0);
  buffer.writeInt16LE(0x0023, 2);
  assert.equal(0x00, buffer[0]);
  assert.equal(0x23, buffer[1]);
  assert.equal(0x23, buffer[2]);
  assert.equal(0x00, buffer[3]);

  buffer.writeInt16BE(-5, 0);
  buffer.writeInt16LE(-5, 2);
  assert.equal(0xff, buffer[0]);
  assert.equal(0xfb, buffer[1]);
  assert.equal(0xfb, buffer[2]);
  assert.equal(0xff, buffer[3]);

  buffer.writeInt16BE(-1679, 1);
  buffer.writeInt16LE(-1679, 3);
  assert.equal(0xf9, buffer[1]);
  assert.equal(0x71, buffer[2]);
  assert.equal(0x71, buffer[3]);
  assert.equal(0xf9, buffer[4]);

  /* Make sure we handle min/max correctly */
  buffer.writeInt16BE(0x7fff, 0);
  buffer.writeInt16BE(-0x8000, 2);
  assert.equal(0x7f, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0x80, buffer[2]);
  assert.equal(0x00, buffer[3]);
  assert.throws(function() {
    buffer.writeInt16BE(0x7fff + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt16BE(-0x8000 - 1, 0);
  });

  buffer.writeInt16LE(0x7fff, 0);
  buffer.writeInt16LE(-0x8000, 2);
  assert.equal(0xff, buffer[0]);
  assert.equal(0x7f, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x80, buffer[3]);
  assert.throws(function() {
    buffer.writeInt16LE(0x7fff + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt16LE(-0x8000 - 1, 0);
  });
}


function test32(clazz) {
  var buffer = new clazz(8);

  buffer.writeInt32BE(0x23, 0);
  buffer.writeInt32LE(0x23, 4);
  assert.equal(0x00, buffer[0]);
  assert.equal(0x00, buffer[1]);
  assert.equal(0x00, buffer[2]);
  assert.equal(0x23, buffer[3]);
  assert.equal(0x23, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);

  buffer.writeInt32BE(-5, 0);
  buffer.writeInt32LE(-5, 4);
  assert.equal(0xff, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xfb, buffer[3]);
  assert.equal(0xfb, buffer[4]);
  assert.equal(0xff, buffer[5]);
  assert.equal(0xff, buffer[6]);
  assert.equal(0xff, buffer[7]);

  buffer.writeInt32BE(-805306713, 0);
  buffer.writeInt32LE(-805306713, 4);
  assert.equal(0xcf, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0xfe, buffer[2]);
  assert.equal(0xa7, buffer[3]);
  assert.equal(0xa7, buffer[4]);
  assert.equal(0xfe, buffer[5]);
  assert.equal(0xff, buffer[6]);
  assert.equal(0xcf, buffer[7]);

  /* Make sure we handle min/max correctly */
  buffer.writeInt32BE(0x7fffffff, 0);
  buffer.writeInt32BE(-0x80000000, 4);
  assert.equal(0x7f, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0xff, buffer[3]);
  assert.equal(0x80, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x00, buffer[7]);
  assert.throws(function() {
    buffer.writeInt32BE(0x7fffffff + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt32BE(-0x80000000 - 1, 0);
  });

  buffer.writeInt32LE(0x7fffffff, 0);
  buffer.writeInt32LE(-0x80000000, 4);
  assert.equal(0xff, buffer[0]);
  assert.equal(0xff, buffer[1]);
  assert.equal(0xff, buffer[2]);
  assert.equal(0x7f, buffer[3]);
  assert.equal(0x00, buffer[4]);
  assert.equal(0x00, buffer[5]);
  assert.equal(0x00, buffer[6]);
  assert.equal(0x80, buffer[7]);
  assert.throws(function() {
    buffer.writeInt32LE(0x7fffffff + 1, 0);
  });
  assert.throws(function() {
    buffer.writeInt32LE(-0x80000000 - 1, 0);
  });
}


test8(Buffer);
test16(Buffer);
test32(Buffer);
