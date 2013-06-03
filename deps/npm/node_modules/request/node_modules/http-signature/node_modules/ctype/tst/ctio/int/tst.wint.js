/*
 * Tests to verify we're writing signed integers correctly
 */
var mod_ctype = require('../../../ctio.js');
var ASSERT = require('assert');

function test8()
{
	var buffer = new Buffer(4);
	mod_ctype.wsint8(0x23, 'big', buffer, 0);
	mod_ctype.wsint8(0x23, 'little', buffer, 1);
	mod_ctype.wsint8(-5, 'big', buffer, 2);
	mod_ctype.wsint8(-5, 'little', buffer, 3);

	ASSERT.equal(0x23, buffer[0]);
	ASSERT.equal(0x23, buffer[1]);
	ASSERT.equal(0xfb, buffer[2]);
	ASSERT.equal(0xfb, buffer[3]);

	/* Make sure we handle truncation correctly */
	ASSERT.throws(function () {
	     mod_ctype.wsint8(0xabc, 'big', buffer, 0);
	});
	ASSERT.throws(function () {
	     mod_ctype.wsint8(0xabc, 'little', buffer, 0);
	});
}

function test16()
{
	var buffer = new Buffer(6);
	mod_ctype.wsint16(0x0023, 'big', buffer, 0);
	mod_ctype.wsint16(0x0023, 'little', buffer, 2);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x23, buffer[1]);
	ASSERT.equal(0x23, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	mod_ctype.wsint16(-5, 'big', buffer, 0);
	mod_ctype.wsint16(-5, 'little', buffer, 2);
	ASSERT.equal(0xff, buffer[0]);
	ASSERT.equal(0xfb, buffer[1]);
	ASSERT.equal(0xfb, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);

	mod_ctype.wsint16(-1679, 'big', buffer, 1);
	mod_ctype.wsint16(-1679, 'little', buffer, 3);
	ASSERT.equal(0xf9, buffer[1]);
	ASSERT.equal(0x71, buffer[2]);
	ASSERT.equal(0x71, buffer[3]);
	ASSERT.equal(0xf9, buffer[4]);
}

function test32()
{
	var buffer = new Buffer(8);
	mod_ctype.wsint32(0x23, 'big', buffer, 0);
	mod_ctype.wsint32(0x23, 'little', buffer, 4);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x23, buffer[3]);
	ASSERT.equal(0x23, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[7]);

	mod_ctype.wsint32(-5, 'big', buffer, 0);
	mod_ctype.wsint32(-5, 'little', buffer, 4);
	ASSERT.equal(0xff, buffer[0]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xfb, buffer[3]);
	ASSERT.equal(0xfb, buffer[4]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[6]);
	ASSERT.equal(0xff, buffer[7]);

	mod_ctype.wsint32(-805306713, 'big', buffer, 0);
	mod_ctype.wsint32(-805306713, 'litle', buffer, 4);
	ASSERT.equal(0xcf, buffer[0]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xfe, buffer[2]);
	ASSERT.equal(0xa7, buffer[3]);
	ASSERT.equal(0xa7, buffer[4]);
	ASSERT.equal(0xfe, buffer[5]);
	ASSERT.equal(0xff, buffer[6]);
	ASSERT.equal(0xcf, buffer[7]);
}

test8();
test16();
test32();
