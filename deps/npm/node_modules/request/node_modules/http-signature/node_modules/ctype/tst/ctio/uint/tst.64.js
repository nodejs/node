/*
 * Test our ability to read and write unsigned 64-bit integers.
 */

var mod_ctype = require('../../../ctio.js');
var ASSERT = require('assert');

function testRead()
{
	var res, data;
	data = new Buffer(10);

	data[0] = 0x32;
	data[1] = 0x65;
	data[2] = 0x42;
	data[3] = 0x56;
	data[4] = 0x23;
	data[5] = 0xff;
	data[6] = 0xff;
	data[7] = 0xff;
	data[8] = 0x89;
	data[9] = 0x11;
	res = mod_ctype.ruint64(data, 'big', 0);
	ASSERT.equal(0x32654256, res[0]);
	ASSERT.equal(0x23ffffff, res[1]);
	res = mod_ctype.ruint64(data, 'big', 1);
	ASSERT.equal(0x65425623, res[0]);
	ASSERT.equal(0xffffff89, res[1]);
	res = mod_ctype.ruint64(data, 'big', 2);
	ASSERT.equal(0x425623ff, res[0]);
	ASSERT.equal(0xffff8911, res[1]);
	res = mod_ctype.ruint64(data, 'little', 0);
	ASSERT.equal(0xffffff23, res[0]);
	ASSERT.equal(0x56426532, res[1]);
	res = mod_ctype.ruint64(data, 'little', 1);
	ASSERT.equal(0x89ffffff, res[0]);
	ASSERT.equal(0x23564265, res[1]);
	res = mod_ctype.ruint64(data, 'little', 2);
	ASSERT.equal(0x1189ffff, res[0]);
	ASSERT.equal(0xff235642, res[1]);

}

function testReadOver()
{
	var res, data;
	data = new Buffer(10);

	data[0] = 0x80;
	data[1] = 0xff;
	data[2] = 0x80;
	data[3] = 0xff;
	data[4] = 0x80;
	data[5] = 0xff;
	data[6] = 0x80;
	data[7] = 0xff;
	data[8] = 0x80;
	data[9] = 0xff;
	res = mod_ctype.ruint64(data, 'big', 0);
	ASSERT.equal(0x80ff80ff, res[0]);
	ASSERT.equal(0x80ff80ff, res[1]);
	res = mod_ctype.ruint64(data, 'big', 1);
	ASSERT.equal(0xff80ff80, res[0]);
	ASSERT.equal(0xff80ff80, res[1]);
	res = mod_ctype.ruint64(data, 'big', 2);
	ASSERT.equal(0x80ff80ff, res[0]);
	ASSERT.equal(0x80ff80ff, res[1]);
	res = mod_ctype.ruint64(data, 'little', 0);
	ASSERT.equal(0xff80ff80, res[0]);
	ASSERT.equal(0xff80ff80, res[1]);
	res = mod_ctype.ruint64(data, 'little', 1);
	ASSERT.equal(0x80ff80ff, res[0]);
	ASSERT.equal(0x80ff80ff, res[1]);
	res = mod_ctype.ruint64(data, 'little', 2);
	ASSERT.equal(0xff80ff80, res[0]);
	ASSERT.equal(0xff80ff80, res[1]);
}

function testWriteZero()
{
	var data, buf;
	buf = new Buffer(10);

	buf.fill(0x66);
	data = [0, 0];
	mod_ctype.wuint64(data, 'big', buf, 0);
	ASSERT.equal(0, buf[0]);
	ASSERT.equal(0, buf[1]);
	ASSERT.equal(0, buf[2]);
	ASSERT.equal(0, buf[3]);
	ASSERT.equal(0, buf[4]);
	ASSERT.equal(0, buf[5]);
	ASSERT.equal(0, buf[6]);
	ASSERT.equal(0, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	data = [0, 0];
	mod_ctype.wuint64(data, 'big', buf, 1);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0, buf[1]);
	ASSERT.equal(0, buf[2]);
	ASSERT.equal(0, buf[3]);
	ASSERT.equal(0, buf[4]);
	ASSERT.equal(0, buf[5]);
	ASSERT.equal(0, buf[6]);
	ASSERT.equal(0, buf[7]);
	ASSERT.equal(0, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	data = [0, 0];
	mod_ctype.wuint64(data, 'big', buf, 2);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x66, buf[1]);
	ASSERT.equal(0, buf[2]);
	ASSERT.equal(0, buf[3]);
	ASSERT.equal(0, buf[4]);
	ASSERT.equal(0, buf[5]);
	ASSERT.equal(0, buf[6]);
	ASSERT.equal(0, buf[7]);
	ASSERT.equal(0, buf[8]);
	ASSERT.equal(0, buf[9]);


	buf.fill(0x66);
	data = [0, 0];
	mod_ctype.wuint64(data, 'little', buf, 0);
	ASSERT.equal(0, buf[0]);
	ASSERT.equal(0, buf[1]);
	ASSERT.equal(0, buf[2]);
	ASSERT.equal(0, buf[3]);
	ASSERT.equal(0, buf[4]);
	ASSERT.equal(0, buf[5]);
	ASSERT.equal(0, buf[6]);
	ASSERT.equal(0, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	data = [0, 0];
	mod_ctype.wuint64(data, 'little', buf, 1);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0, buf[1]);
	ASSERT.equal(0, buf[2]);
	ASSERT.equal(0, buf[3]);
	ASSERT.equal(0, buf[4]);
	ASSERT.equal(0, buf[5]);
	ASSERT.equal(0, buf[6]);
	ASSERT.equal(0, buf[7]);
	ASSERT.equal(0, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	data = [0, 0];
	mod_ctype.wuint64(data, 'little', buf, 2);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x66, buf[1]);
	ASSERT.equal(0, buf[2]);
	ASSERT.equal(0, buf[3]);
	ASSERT.equal(0, buf[4]);
	ASSERT.equal(0, buf[5]);
	ASSERT.equal(0, buf[6]);
	ASSERT.equal(0, buf[7]);
	ASSERT.equal(0, buf[8]);
	ASSERT.equal(0, buf[9]);
}

/*
 * Also include tests that are going to force us to go into a negative value and
 * insure that it's written correctly.
 */
function testWrite()
{
	var data, buf;

	buf = new Buffer(10);
	data = [ 0x234456, 0x87 ];
	buf.fill(0x66);
	mod_ctype.wuint64(data, 'big', buf, 0);
	ASSERT.equal(0x00, buf[0]);
	ASSERT.equal(0x23, buf[1]);
	ASSERT.equal(0x44, buf[2]);
	ASSERT.equal(0x56, buf[3]);
	ASSERT.equal(0x00, buf[4]);
	ASSERT.equal(0x00, buf[5]);
	ASSERT.equal(0x00, buf[6]);
	ASSERT.equal(0x87, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'big', buf, 1);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x00, buf[1]);
	ASSERT.equal(0x23, buf[2]);
	ASSERT.equal(0x44, buf[3]);
	ASSERT.equal(0x56, buf[4]);
	ASSERT.equal(0x00, buf[5]);
	ASSERT.equal(0x00, buf[6]);
	ASSERT.equal(0x00, buf[7]);
	ASSERT.equal(0x87, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'big', buf, 2);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x66, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x23, buf[3]);
	ASSERT.equal(0x44, buf[4]);
	ASSERT.equal(0x56, buf[5]);
	ASSERT.equal(0x00, buf[6]);
	ASSERT.equal(0x00, buf[7]);
	ASSERT.equal(0x00, buf[8]);
	ASSERT.equal(0x87, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'little', buf, 0);
	ASSERT.equal(0x87, buf[0]);
	ASSERT.equal(0x00, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x00, buf[3]);
	ASSERT.equal(0x56, buf[4]);
	ASSERT.equal(0x44, buf[5]);
	ASSERT.equal(0x23, buf[6]);
	ASSERT.equal(0x00, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'little', buf, 1);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x87, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x00, buf[3]);
	ASSERT.equal(0x00, buf[4]);
	ASSERT.equal(0x56, buf[5]);
	ASSERT.equal(0x44, buf[6]);
	ASSERT.equal(0x23, buf[7]);
	ASSERT.equal(0x00, buf[8]);
	ASSERT.equal(0x66, buf[9]);


	buf.fill(0x66);
	mod_ctype.wuint64(data, 'little', buf, 2);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x66, buf[1]);
	ASSERT.equal(0x87, buf[2]);
	ASSERT.equal(0x00, buf[3]);
	ASSERT.equal(0x00, buf[4]);
	ASSERT.equal(0x00, buf[5]);
	ASSERT.equal(0x56, buf[6]);
	ASSERT.equal(0x44, buf[7]);
	ASSERT.equal(0x23, buf[8]);
	ASSERT.equal(0x00, buf[9]);

	data = [0xffff3421, 0x34abcdba];
	buf.fill(0x66);
	mod_ctype.wuint64(data, 'big', buf, 0);
	ASSERT.equal(0xff, buf[0]);
	ASSERT.equal(0xff, buf[1]);
	ASSERT.equal(0x34, buf[2]);
	ASSERT.equal(0x21, buf[3]);
	ASSERT.equal(0x34, buf[4]);
	ASSERT.equal(0xab, buf[5]);
	ASSERT.equal(0xcd, buf[6]);
	ASSERT.equal(0xba, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'big', buf, 1);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0xff, buf[1]);
	ASSERT.equal(0xff, buf[2]);
	ASSERT.equal(0x34, buf[3]);
	ASSERT.equal(0x21, buf[4]);
	ASSERT.equal(0x34, buf[5]);
	ASSERT.equal(0xab, buf[6]);
	ASSERT.equal(0xcd, buf[7]);
	ASSERT.equal(0xba, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'big', buf, 2);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x66, buf[1]);
	ASSERT.equal(0xff, buf[2]);
	ASSERT.equal(0xff, buf[3]);
	ASSERT.equal(0x34, buf[4]);
	ASSERT.equal(0x21, buf[5]);
	ASSERT.equal(0x34, buf[6]);
	ASSERT.equal(0xab, buf[7]);
	ASSERT.equal(0xcd, buf[8]);
	ASSERT.equal(0xba, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'little', buf, 0);
	ASSERT.equal(0xba, buf[0]);
	ASSERT.equal(0xcd, buf[1]);
	ASSERT.equal(0xab, buf[2]);
	ASSERT.equal(0x34, buf[3]);
	ASSERT.equal(0x21, buf[4]);
	ASSERT.equal(0x34, buf[5]);
	ASSERT.equal(0xff, buf[6]);
	ASSERT.equal(0xff, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'little', buf, 1);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0xba, buf[1]);
	ASSERT.equal(0xcd, buf[2]);
	ASSERT.equal(0xab, buf[3]);
	ASSERT.equal(0x34, buf[4]);
	ASSERT.equal(0x21, buf[5]);
	ASSERT.equal(0x34, buf[6]);
	ASSERT.equal(0xff, buf[7]);
	ASSERT.equal(0xff, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wuint64(data, 'little', buf, 2);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x66, buf[1]);
	ASSERT.equal(0xba, buf[2]);
	ASSERT.equal(0xcd, buf[3]);
	ASSERT.equal(0xab, buf[4]);
	ASSERT.equal(0x34, buf[5]);
	ASSERT.equal(0x21, buf[6]);
	ASSERT.equal(0x34, buf[7]);
	ASSERT.equal(0xff, buf[8]);
	ASSERT.equal(0xff, buf[9]);
}

/*
 * Make sure we catch invalid writes.
 */
function testWriteInvalid()
{
	var data, buf;

	/* Buffer too small */
	buf = new Buffer(4);
	data = [ 0, 0];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 0);
	}, Error, 'buffer too small');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 0);
	}, Error, 'buffer too small');

	/* Beyond the end of the buffer */
	buf = new Buffer(12);
	data = [ 0, 0];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 11);
	}, Error, 'write beyond end of buffer');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 11);
	}, Error, 'write beyond end of buffer');

	/* Write negative values */
	buf = new Buffer(12);
	data = [ -3, 0 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write negative number');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write negative number');

	data = [ 0, -3 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write negative number');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write negative number');

	data = [ -3, -3 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write negative number');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write negative number');


	/* Write fractional values */
	buf = new Buffer(12);
	data = [ 3.33, 0 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ 0, 3.3 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ 3.33, 2.42 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	/* Write values that are too large */
	buf = new Buffer(12);
	data = [ 0xffffffffff, 23 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ 0xffffffffff, 0xffffff238 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ 0x23, 0xffffff238 ];
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wuint64(data, 'little', buf, 1);
	}, Error, 'write too large');
}


testRead();
testReadOver();
testWriteZero();
testWrite();
testWriteInvalid();
