/*
 * A battery of tests for sucessful round-trip between writes and reads
 */

var mod_ctype = require('../../../ctio.js');
var ASSERT = require('assert');


/*
 * What the heck, let's just test every value for 8-bits.
 */

function test8() {
	var data = new Buffer(1);
	var i;
	for (i = 0; i < 256; i++) {
		mod_ctype.wuint8(i, 'big', data, 0);
		ASSERT.equal(i, mod_ctype.ruint8(data, 'big', 0));
		mod_ctype.wuint8(i, 'little', data, 0);
		ASSERT.equal(i, mod_ctype.ruint8(data, 'little', 0));
	}
	ASSERT.ok(true);
}

/*
 * Test a random sample of 256 values in the 16-bit unsigned range
 */

function test16() {
	var data = new Buffer(2);
	var i = 0;
	for (i = 0; i < 256; i++) {
		var value = Math.round(Math.random() * Math.pow(2, 16));
		mod_ctype.wuint16(value, 'big', data, 0);
		ASSERT.equal(value, mod_ctype.ruint16(data, 'big', 0));
		mod_ctype.wuint16(value, 'little', data, 0);
		ASSERT.equal(value, mod_ctype.ruint16(data, 'little', 0));
	}
}

/*
 * Test a random sample of 256 values in the 32-bit unsigned range
 */

function test32() {
	var data = new Buffer(4);
	var i = 0;
	for (i = 0; i < 256; i++) {
		var value = Math.round(Math.random() * Math.pow(2, 32));
		mod_ctype.wuint32(value, 'big', data, 0);
		ASSERT.equal(value, mod_ctype.ruint32(data, 'big', 0));
		mod_ctype.wuint32(value, 'little', data, 0);
		ASSERT.equal(value, mod_ctype.ruint32(data, 'little', 0));
	}
}

/*
 * Test a random sample of 256 values in the 64-bit unsigned range
 */

function test64() {
	var data = new Buffer(8);
	var i = 0;
	for (i = 0; i < 256; i++) {
		var low = Math.round(Math.random() * Math.pow(2, 32));
		var high = Math.round(Math.random() * Math.pow(2, 32));
		mod_ctype.wuint64([high, low], 'big', data, 0);
		var result = mod_ctype.ruint64(data, 'big', 0);
		ASSERT.equal(high, result[0]);
		ASSERT.equal(low, result[1]);
		mod_ctype.wuint64([high, low], 'little', data, 0);
		result = mod_ctype.ruint64(data, 'little', 0);
		ASSERT.equal(high, result[0]);
		ASSERT.equal(low, result[1]);
	}
}

exports.test8 = test8;
exports.test16 = test16;
exports.test32 = test32;
exports.test64 = test64;
