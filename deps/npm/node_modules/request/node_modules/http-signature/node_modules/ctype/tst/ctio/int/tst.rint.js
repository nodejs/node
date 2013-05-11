/*
 * Tests to verify we're reading in signed integers correctly
 */
var mod_ctype = require('../../../ctio.js');
var ASSERT = require('assert');

/*
 * Test 8 bit signed integers
 */
function test8()
{
	var data = new Buffer(4);

	data[0] = 0x23;
	ASSERT.equal(0x23, mod_ctype.rsint8(data, 'big', 0));
	ASSERT.equal(0x23, mod_ctype.rsint8(data, 'little', 0));

	data[0] = 0xff;
	ASSERT.equal(-1, mod_ctype.rsint8(data, 'big', 0));
	ASSERT.equal(-1, mod_ctype.rsint8(data, 'little', 0));

	data[0] = 0x87;
	data[1] = 0xab;
	data[2] = 0x7c;
	data[3] = 0xef;
	ASSERT.equal(-121, mod_ctype.rsint8(data, 'big', 0));
	ASSERT.equal(-85, mod_ctype.rsint8(data, 'big', 1));
	ASSERT.equal(124, mod_ctype.rsint8(data, 'big', 2));
	ASSERT.equal(-17, mod_ctype.rsint8(data, 'big', 3));
	ASSERT.equal(-121, mod_ctype.rsint8(data, 'little', 0));
	ASSERT.equal(-85, mod_ctype.rsint8(data, 'little', 1));
	ASSERT.equal(124, mod_ctype.rsint8(data, 'little', 2));
	ASSERT.equal(-17, mod_ctype.rsint8(data, 'little', 3));
}

function test16()
{
	var buffer = new Buffer(6);
	buffer[0] = 0x16;
	buffer[1] = 0x79;
	ASSERT.equal(0x1679, mod_ctype.rsint16(buffer, 'big', 0));
	ASSERT.equal(0x7916, mod_ctype.rsint16(buffer, 'little', 0));

	buffer[0] = 0xff;
	buffer[1] = 0x80;
	ASSERT.equal(-128, mod_ctype.rsint16(buffer, 'big', 0));
	ASSERT.equal(-32513, mod_ctype.rsint16(buffer, 'little', 0));

	/* test offset with weenix */
	buffer[0] = 0x77;
	buffer[1] = 0x65;
	buffer[2] = 0x65;
	buffer[3] = 0x6e;
	buffer[4] = 0x69;
	buffer[5] = 0x78;
	ASSERT.equal(0x7765, mod_ctype.rsint16(buffer, 'big', 0));
	ASSERT.equal(0x6565, mod_ctype.rsint16(buffer, 'big', 1));
	ASSERT.equal(0x656e, mod_ctype.rsint16(buffer, 'big', 2));
	ASSERT.equal(0x6e69, mod_ctype.rsint16(buffer, 'big', 3));
	ASSERT.equal(0x6978, mod_ctype.rsint16(buffer, 'big', 4));
	ASSERT.equal(0x6577, mod_ctype.rsint16(buffer, 'little', 0));
	ASSERT.equal(0x6565, mod_ctype.rsint16(buffer, 'little', 1));
	ASSERT.equal(0x6e65, mod_ctype.rsint16(buffer, 'little', 2));
	ASSERT.equal(0x696e, mod_ctype.rsint16(buffer, 'little', 3));
	ASSERT.equal(0x7869, mod_ctype.rsint16(buffer, 'little', 4));
}

function test32()
{
	var buffer = new Buffer(6);
	buffer[0] = 0x43;
	buffer[1] = 0x53;
	buffer[2] = 0x16;
	buffer[3] = 0x79;
	ASSERT.equal(0x43531679, mod_ctype.rsint32(buffer, 'big', 0));
	ASSERT.equal(0x79165343, mod_ctype.rsint32(buffer, 'little', 0));

	buffer[0] = 0xff;
	buffer[1] = 0xfe;
	buffer[2] = 0xef;
	buffer[3] = 0xfa;
	ASSERT.equal(-69638, mod_ctype.rsint32(buffer, 'big', 0));
	ASSERT.equal(-84934913, mod_ctype.rsint32(buffer, 'little', 0));

	buffer[0] = 0x42;
	buffer[1] = 0xc3;
	buffer[2] = 0x95;
	buffer[3] = 0xa9;
	buffer[4] = 0x36;
	buffer[5] = 0x17;
	ASSERT.equal(0x42c395a9, mod_ctype.rsint32(buffer, 'big', 0));
	ASSERT.equal(-1013601994, mod_ctype.rsint32(buffer, 'big', 1));
	ASSERT.equal(-1784072681, mod_ctype.rsint32(buffer, 'big', 2));
	ASSERT.equal(-1449802942, mod_ctype.rsint32(buffer, 'little', 0));
	ASSERT.equal(917083587, mod_ctype.rsint32(buffer, 'little', 1));
	ASSERT.equal(389458325, mod_ctype.rsint32(buffer, 'little', 2));
}

test8();
test16();
test32();
