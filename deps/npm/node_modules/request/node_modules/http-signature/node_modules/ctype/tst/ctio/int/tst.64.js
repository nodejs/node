/*
 * Test our ability to read and write signed 64-bit integers.
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
	res = mod_ctype.rsint64(data, 'big', 0);
	ASSERT.equal(0x32654256, res[0]);
	ASSERT.equal(0x23ffffff, res[1]);
	res = mod_ctype.rsint64(data, 'big', 1);
	ASSERT.equal(0x65425623, res[0]);
	ASSERT.equal(0xffffff89, res[1]);
	res = mod_ctype.rsint64(data, 'big', 2);
	ASSERT.equal(0x425623ff, res[0]);
	ASSERT.equal(0xffff8911, res[1]);
	res = mod_ctype.rsint64(data, 'little', 0);
	ASSERT.equal(-0x000000dc, res[0]);
	ASSERT.equal(-0xa9bd9ace, res[1]);
	res = mod_ctype.rsint64(data, 'little', 1);
	ASSERT.equal(-0x76000000, res[0]);
	ASSERT.equal(-0xdca9bd9b, res[1]);
	res = mod_ctype.rsint64(data, 'little', 2);
	ASSERT.equal(0x1189ffff, res[0]);
	ASSERT.equal(0xff235642, res[1]);

	data.fill(0x00);
	res = mod_ctype.rsint64(data, 'big', 0);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(0x00000000, res[1]);
	res = mod_ctype.rsint64(data, 'big', 1);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(0x00000000, res[1]);
	res = mod_ctype.rsint64(data, 'big', 2);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(0x00000000, res[1]);
	res = mod_ctype.rsint64(data, 'little', 0);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(0x00000000, res[1]);
	res = mod_ctype.rsint64(data, 'little', 1);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(0x00000000, res[1]);
	res = mod_ctype.rsint64(data, 'little', 2);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(0x00000000, res[1]);

	data.fill(0xff);
	res = mod_ctype.rsint64(data, 'big', 0);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(-1, res[1]);
	res = mod_ctype.rsint64(data, 'big', 1);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(-1, res[1]);
	res = mod_ctype.rsint64(data, 'big', 2);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(-1, res[1]);
	res = mod_ctype.rsint64(data, 'little', 0);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(-1, res[1]);
	res = mod_ctype.rsint64(data, 'little', 1);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(-1, res[1]);
	res = mod_ctype.rsint64(data, 'little', 2);
	ASSERT.equal(0x00000000, res[0]);
	ASSERT.equal(-1, res[1]);

	data[0] = 0x80;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	data[4] = 0x00;
	data[5] = 0x00;
	data[6] = 0x00;
	data[7] = 0x00;
	res = mod_ctype.rsint64(data, 'big', 0);
	ASSERT.equal(-0x80000000, res[0]);
	ASSERT.equal(0, res[1]);


	data[7] = 0x80;
	data[6] = 0x00;
	data[5] = 0x00;
	data[4] = 0x00;
	data[3] = 0x00;
	data[2] = 0x00;
	data[1] = 0x00;
	data[0] = 0x00;
	res = mod_ctype.rsint64(data, 'little', 0);
	ASSERT.equal(-0x80000000, res[0]);
	ASSERT.equal(0, res[1]);

	data[0] = 0x80;
	data[1] = 0x00;
	data[2] = 0x00;
	data[3] = 0x00;
	data[4] = 0x00;
	data[5] = 0x00;
	data[6] = 0x00;
	data[7] = 0x01;
	res = mod_ctype.rsint64(data, 'big', 0);
	ASSERT.equal(-0x7fffffff, res[0]);
	ASSERT.equal(-0xffffffff, res[1]);


}

function testWriteZero()
{
	var data, buf;
	buf = new Buffer(10);

	buf.fill(0x66);
	data = [0, 0];
	mod_ctype.wsint64(data, 'big', buf, 0);
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
	mod_ctype.wsint64(data, 'big', buf, 1);
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
	mod_ctype.wsint64(data, 'big', buf, 2);
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
	mod_ctype.wsint64(data, 'little', buf, 0);
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
	mod_ctype.wsint64(data, 'little', buf, 1);
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
	mod_ctype.wsint64(data, 'little', buf, 2);
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
	mod_ctype.wsint64(data, 'big', buf, 0);
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
	mod_ctype.wsint64(data, 'big', buf, 1);
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
	mod_ctype.wsint64(data, 'big', buf, 2);
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
	mod_ctype.wsint64(data, 'little', buf, 0);
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
	mod_ctype.wsint64(data, 'little', buf, 1);
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
	mod_ctype.wsint64(data, 'little', buf, 2);
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

	data = [0x3421, 0x34abcdba];
	buf.fill(0x66);
	mod_ctype.wsint64(data, 'big', buf, 0);
	ASSERT.equal(0x00, buf[0]);
	ASSERT.equal(0x00, buf[1]);
	ASSERT.equal(0x34, buf[2]);
	ASSERT.equal(0x21, buf[3]);
	ASSERT.equal(0x34, buf[4]);
	ASSERT.equal(0xab, buf[5]);
	ASSERT.equal(0xcd, buf[6]);
	ASSERT.equal(0xba, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wsint64(data, 'big', buf, 1);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x00, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x34, buf[3]);
	ASSERT.equal(0x21, buf[4]);
	ASSERT.equal(0x34, buf[5]);
	ASSERT.equal(0xab, buf[6]);
	ASSERT.equal(0xcd, buf[7]);
	ASSERT.equal(0xba, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wsint64(data, 'big', buf, 2);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x66, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x00, buf[3]);
	ASSERT.equal(0x34, buf[4]);
	ASSERT.equal(0x21, buf[5]);
	ASSERT.equal(0x34, buf[6]);
	ASSERT.equal(0xab, buf[7]);
	ASSERT.equal(0xcd, buf[8]);
	ASSERT.equal(0xba, buf[9]);

	buf.fill(0x66);
	mod_ctype.wsint64(data, 'little', buf, 0);
	ASSERT.equal(0xba, buf[0]);
	ASSERT.equal(0xcd, buf[1]);
	ASSERT.equal(0xab, buf[2]);
	ASSERT.equal(0x34, buf[3]);
	ASSERT.equal(0x21, buf[4]);
	ASSERT.equal(0x34, buf[5]);
	ASSERT.equal(0x00, buf[6]);
	ASSERT.equal(0x00, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wsint64(data, 'little', buf, 1);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0xba, buf[1]);
	ASSERT.equal(0xcd, buf[2]);
	ASSERT.equal(0xab, buf[3]);
	ASSERT.equal(0x34, buf[4]);
	ASSERT.equal(0x21, buf[5]);
	ASSERT.equal(0x34, buf[6]);
	ASSERT.equal(0x00, buf[7]);
	ASSERT.equal(0x00, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wsint64(data, 'little', buf, 2);
	ASSERT.equal(0x66, buf[0]);
	ASSERT.equal(0x66, buf[1]);
	ASSERT.equal(0xba, buf[2]);
	ASSERT.equal(0xcd, buf[3]);
	ASSERT.equal(0xab, buf[4]);
	ASSERT.equal(0x34, buf[5]);
	ASSERT.equal(0x21, buf[6]);
	ASSERT.equal(0x34, buf[7]);
	ASSERT.equal(0x00, buf[8]);
	ASSERT.equal(0x00, buf[9]);


	data = [ -0x80000000, 0 ];
	buf.fill(0x66);
	mod_ctype.wsint64(data, 'big', buf, 0);
	ASSERT.equal(0x80, buf[0]);
	ASSERT.equal(0x00, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x00, buf[3]);
	ASSERT.equal(0x00, buf[4]);
	ASSERT.equal(0x00, buf[5]);
	ASSERT.equal(0x00, buf[6]);
	ASSERT.equal(0x00, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wsint64(data, 'little', buf, 0);
	ASSERT.equal(0x00, buf[0]);
	ASSERT.equal(0x00, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x00, buf[3]);
	ASSERT.equal(0x00, buf[4]);
	ASSERT.equal(0x00, buf[5]);
	ASSERT.equal(0x00, buf[6]);
	ASSERT.equal(0x80, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	data = [ -0x7fffffff, -0xffffffff ];
	buf.fill(0x66);
	mod_ctype.wsint64(data, 'big', buf, 0);
	ASSERT.equal(0x80, buf[0]);
	ASSERT.equal(0x00, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x00, buf[3]);
	ASSERT.equal(0x00, buf[4]);
	ASSERT.equal(0x00, buf[5]);
	ASSERT.equal(0x00, buf[6]);
	ASSERT.equal(0x01, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wsint64(data, 'little', buf, 0);
	ASSERT.equal(0x01, buf[0]);
	ASSERT.equal(0x00, buf[1]);
	ASSERT.equal(0x00, buf[2]);
	ASSERT.equal(0x00, buf[3]);
	ASSERT.equal(0x00, buf[4]);
	ASSERT.equal(0x00, buf[5]);
	ASSERT.equal(0x00, buf[6]);
	ASSERT.equal(0x80, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	data = [ 0x0, -0x1];
	buf.fill(0x66);
	mod_ctype.wsint64(data, 'big', buf, 0);
	ASSERT.equal(0xff, buf[0]);
	ASSERT.equal(0xff, buf[1]);
	ASSERT.equal(0xff, buf[2]);
	ASSERT.equal(0xff, buf[3]);
	ASSERT.equal(0xff, buf[4]);
	ASSERT.equal(0xff, buf[5]);
	ASSERT.equal(0xff, buf[6]);
	ASSERT.equal(0xff, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);

	buf.fill(0x66);
	mod_ctype.wsint64(data, 'little', buf, 0);
	ASSERT.equal(0xff, buf[0]);
	ASSERT.equal(0xff, buf[1]);
	ASSERT.equal(0xff, buf[2]);
	ASSERT.equal(0xff, buf[3]);
	ASSERT.equal(0xff, buf[4]);
	ASSERT.equal(0xff, buf[5]);
	ASSERT.equal(0xff, buf[6]);
	ASSERT.equal(0xff, buf[7]);
	ASSERT.equal(0x66, buf[8]);
	ASSERT.equal(0x66, buf[9]);
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
	    mod_ctype.wsint64(data, 'big', buf, 0);
	}, Error, 'buffer too small');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 0);
	}, Error, 'buffer too small');

	/* Beyond the end of the buffer */
	buf = new Buffer(12);
	data = [ 0, 0];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 11);
	}, Error, 'write beyond end of buffer');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 11);
	}, Error, 'write beyond end of buffer');

	/* Write fractional values */
	buf = new Buffer(12);
	data = [ 3.33, 0 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ 0, 3.3 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ -3.33, 0 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ 0, -3.3 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ 3.33, 2.42 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ 3.33, -2.42 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ -3.33, -2.42 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	data = [ -3.33, 2.42 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write fractions');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write fractions');

	/* Signs don't match */
	buf = new Buffer(12);
	data = [ 0x800000, -0x32 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ -0x800000, 0x32 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	/* Write values that are too large */
	buf = new Buffer(12);
	data = [ 0x80000000, 0 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ 0x7fffffff, 0x100000000 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ 0x00, 0x800000000 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ 0xffffffffff, 0xffffff238 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ 0x23, 0xffffff238 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ -0x80000000, -0xfff238 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');

	data = [ -0x80000004, -0xfff238 ];
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'big', buf, 1);
	}, Error, 'write too large');
	ASSERT.throws(function () {
	    mod_ctype.wsint64(data, 'little', buf, 1);
	}, Error, 'write too large');
}


testRead();
testWrite();
testWriteZero();
testWriteInvalid();
