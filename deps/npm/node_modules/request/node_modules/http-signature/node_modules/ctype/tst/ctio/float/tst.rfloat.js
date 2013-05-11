/*
 * Battery of tests to break our floating point implementation. Oh ho ho.
 *
 * There are a few useful ways to generate the expected output. The first is
 * just write a C program and write raw bytes out and inspect with xxd. Remember
 * to consider whether or not you're on a big endian or little endian machine.
 * Another useful site I found to help with some of this was:
 *
 * http://babbage.cs.qc.edu/IEEE-754/
 */

var mod_ctype = require('../../../ctio.js');
var ASSERT = require('assert');

function testfloat()
{
	var buffer = new Buffer(4);
	/* Start off with some of the easy ones: +zero */
	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0;

	ASSERT.equal(0, mod_ctype.rfloat(buffer, 'big', 0));
	ASSERT.equal(0, mod_ctype.rfloat(buffer, 'little', 0));

	/* Test -0 */
	buffer[0] = 0x80;
	ASSERT.equal(0, mod_ctype.rfloat(buffer, 'big', 0));
	buffer[3] = buffer[0];
	buffer[0] = 0;
	ASSERT.equal(0, mod_ctype.rfloat(buffer, 'little', 0));

	/* Catch +infin */
	buffer[0] = 0x7f;
	buffer[1] = 0x80;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	ASSERT.equal(Number.POSITIVE_INFINITY,
	    mod_ctype.rfloat(buffer, 'big', 0));
	buffer[3] = 0x7f;
	buffer[2] = 0x80;
	buffer[1] = 0x00;
	buffer[0] = 0x00;
	ASSERT.equal(Number.POSITIVE_INFINITY,
	    mod_ctype.rfloat(buffer, 'litle', 0));

	/* Catch -infin */
	buffer[0] = 0xff;
	buffer[1] = 0x80;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	ASSERT.equal(Number.NEGATIVE_INFINITY,
	    mod_ctype.rfloat(buffer, 'big', 0));
	buffer[3] = 0xff;
	buffer[2] = 0x80;
	buffer[1] = 0x00;
	buffer[0] = 0x00;
	ASSERT.equal(Number.NEGATIVE_INFINITY,
	    mod_ctype.rfloat(buffer, 'litle', 0));

	/* Catch NaN */

	buffer[0] = 0x7f;
	buffer[1] = 0x80;
	buffer[2] = 0x00;
	buffer[3] = 0x23;
	ASSERT.ok(isNaN(mod_ctype.rfloat(buffer, 'big', 0)));
	buffer[3] = 0x7f;
	buffer[2] = 0x80;
	buffer[1] = 0x00;
	buffer[0] = 0x23;
	ASSERT.ok(isNaN(mod_ctype.rfloat(buffer, 'little', 0)));

	/* Catch -infin */
	buffer[0] = 0xff;
	buffer[1] = 0x80;
	buffer[2] = 0x00;
	buffer[3] = 0x23;
	ASSERT.ok(isNaN(mod_ctype.rfloat(buffer, 'big', 0)));
	buffer[3] = 0xff;
	buffer[2] = 0x80;
	buffer[1] = 0x00;
	buffer[0] = 0x23;
	ASSERT.ok(isNaN(mod_ctype.rfloat(buffer, 'little', 0)));

	/* On to some basic tests */
	/* 1.125 */
	buffer[0] = 0x3f;
	buffer[1] = 0x90;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	ASSERT.equal(1.125, mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x3f;
	buffer[2] = 0x90;
	buffer[1] = 0x00;
	buffer[0] = 0x00;
	ASSERT.equal(1.125, mod_ctype.rfloat(buffer, 'little', 0));

	/* ff34a2b0 -2.4010576103645774e+38 */
	buffer[0] = 0xff;
	buffer[1] = 0x34;
	buffer[2] = 0xa2;
	buffer[3] = 0xb0;
	ASSERT.equal(-2.4010576103645774e+38,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0xff;
	buffer[2] = 0x34;
	buffer[1] = 0xa2;
	buffer[0] = 0xb0;
	ASSERT.equal(-2.4010576103645774e+38,
	    mod_ctype.rfloat(buffer, 'little', 0));

	/* Denormalized tests */

	/* 0003f89a +/- 3.6468792534053364e-40 */
	buffer[0] = 0x00;
	buffer[1] = 0x03;
	buffer[2] = 0xf8;
	buffer[3] = 0x9a;
	ASSERT.equal(3.6468792534053364e-40,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x00;
	buffer[2] = 0x03;
	buffer[1] = 0xf8;
	buffer[0] = 0x9a;
	ASSERT.equal(3.6468792534053364e-40,
	    mod_ctype.rfloat(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0x03;
	buffer[2] = 0xf8;
	buffer[3] = 0x9a;
	ASSERT.equal(-3.6468792534053364e-40,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x80;
	buffer[2] = 0x03;
	buffer[1] = 0xf8;
	buffer[0] = 0x9a;
	ASSERT.equal(-3.6468792534053364e-40,
	    mod_ctype.rfloat(buffer, 'little', 0));


	/* Maximum and minimum normalized and denormalized values */

	/* Largest normalized number +/- 3.4028234663852886e+38 */

	buffer[0] = 0x7f;
	buffer[1] = 0x7f;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	ASSERT.equal(3.4028234663852886e+38,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x7f;
	buffer[2] = 0x7f;
	buffer[1] = 0xff;
	buffer[0] = 0xff;
	ASSERT.equal(3.4028234663852886e+38,
	    mod_ctype.rfloat(buffer, 'little', 0));

	buffer[0] = 0xff;
	buffer[1] = 0x7f;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	ASSERT.equal(-3.4028234663852886e+38,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0xff;
	buffer[2] = 0x7f;
	buffer[1] = 0xff;
	buffer[0] = 0xff;
	ASSERT.equal(-3.4028234663852886e+38,
	    mod_ctype.rfloat(buffer, 'little', 0));

	/* Smallest normalied number +/- 1.1754943508222875e-38 */
	buffer[0] = 0x00;
	buffer[1] = 0x80;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	ASSERT.equal(1.1754943508222875e-38,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x00;
	buffer[2] = 0x80;
	buffer[1] = 0x00;
	buffer[0] = 0x00;
	ASSERT.equal(1.1754943508222875e-38,
	    mod_ctype.rfloat(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0x80;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	ASSERT.equal(-1.1754943508222875e-38,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x80;
	buffer[2] = 0x80;
	buffer[1] = 0x00;
	buffer[0] = 0x00;
	ASSERT.equal(-1.1754943508222875e-38,
	    mod_ctype.rfloat(buffer, 'little', 0));


	/* Smallest denormalized number 1.401298464324817e-45 */
	buffer[0] = 0x00;
	buffer[1] = 0x00;
	buffer[2] = 0x00;
	buffer[3] = 0x01;
	ASSERT.equal(1.401298464324817e-45,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x00;
	buffer[2] = 0x00;
	buffer[1] = 0x00;
	buffer[0] = 0x01;
	ASSERT.equal(1.401298464324817e-45,
	    mod_ctype.rfloat(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0x00;
	buffer[2] = 0x00;
	buffer[3] = 0x01;
	ASSERT.equal(-1.401298464324817e-45,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x80;
	buffer[2] = 0x00;
	buffer[1] = 0x00;
	buffer[0] = 0x01;
	ASSERT.equal(-1.401298464324817e-45,
	    mod_ctype.rfloat(buffer, 'little', 0));

	/* Largest denormalized value +/- 1.1754942106924411e-38 */
	buffer[0] = 0x00;
	buffer[1] = 0x7f;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	ASSERT.equal(1.1754942106924411e-38,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x00;
	buffer[2] = 0x7f;
	buffer[1] = 0xff;
	buffer[0] = 0xff;
	ASSERT.equal(1.1754942106924411e-38,
	    mod_ctype.rfloat(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0x7f;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	ASSERT.equal(-1.1754942106924411e-38,
	    mod_ctype.rfloat(buffer, 'big', 0));

	buffer[3] = 0x80;
	buffer[2] = 0x7f;
	buffer[1] = 0xff;
	buffer[0] = 0xff;
	ASSERT.equal(-1.1754942106924411e-38,
	    mod_ctype.rfloat(buffer, 'little', 0));

	/* Do some quick offset testing */
	buffer = new Buffer(6);
	buffer[0] = 0x7f;
	buffer[1] = 0x4e;
	buffer[2] = 0x8a;
	buffer[3] = 0x79;
	buffer[4] = 0xcd;
	buffer[5] = 0x3f;

	ASSERT.equal(2.745399582697325e+38,
	    mod_ctype.rfloat(buffer, 'big', 0));
	ASSERT.equal(1161619072,
	    mod_ctype.rfloat(buffer, 'big', 1));
	ASSERT.equal(-1.2027516403607578e-32,
	    mod_ctype.rfloat(buffer, 'big', 2));

	ASSERT.equal(8.97661320504413e+34,
	    mod_ctype.rfloat(buffer, 'little', 0));
	ASSERT.equal(-261661920,
	    mod_ctype.rfloat(buffer, 'little', 1));
	ASSERT.equal(1.605271577835083,
	    mod_ctype.rfloat(buffer, 'little', 2));

}

function testdouble()
{
	var buffer = new Buffer(10);

	/* Check 0 */
	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 0;
	ASSERT.equal(0,
	    mod_ctype.rdouble(buffer, 'big', 0));
	ASSERT.equal(0,
	    mod_ctype.rdouble(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 0;
	ASSERT.equal(0,
	    mod_ctype.rdouble(buffer, 'big', 0));
	buffer[7] = 0x80;
	buffer[6] = 0;
	buffer[5] = 0;
	buffer[4] = 0;
	buffer[3] = 0;
	buffer[2] = 0;
	buffer[1] = 0;
	buffer[0] = 0;
	ASSERT.equal(0,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* Check NaN */
	buffer[0] = 0x7f;
	buffer[1] = 0xf0;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 23;
	ASSERT.ok(isNaN(mod_ctype.rdouble(buffer, 'big', 0)));

	buffer[7] = 0x7f;
	buffer[6] = 0xf0;
	buffer[5] = 0;
	buffer[4] = 0;
	buffer[3] = 0;
	buffer[2] = 0;
	buffer[1] = 0;
	buffer[0] = 23;
	ASSERT.ok(isNaN(mod_ctype.rdouble(buffer, 'little', 0)));

	buffer[0] = 0xff;
	buffer[1] = 0xf0;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 23;
	ASSERT.ok(isNaN(mod_ctype.rdouble(buffer, 'big', 0)));

	buffer[7] = 0xff;
	buffer[6] = 0xf0;
	buffer[5] = 0;
	buffer[4] = 0;
	buffer[3] = 0;
	buffer[2] = 0;
	buffer[1] = 0;
	buffer[0] = 23;
	ASSERT.ok(isNaN(mod_ctype.rdouble(buffer, 'little', 0)));

	/* pos inf */
	buffer[0] = 0x7f;
	buffer[1] = 0xf0;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 0;
	ASSERT.equal(Number.POSITIVE_INFINITY,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x7f;
	buffer[6] = 0xf0;
	buffer[5] = 0;
	buffer[4] = 0;
	buffer[3] = 0;
	buffer[2] = 0;
	buffer[1] = 0;
	buffer[0] = 0;
	ASSERT.equal(Number.POSITIVE_INFINITY,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* neg inf */
	buffer[0] = 0xff;
	buffer[1] = 0xf0;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 0;
	ASSERT.equal(Number.NEGATIVE_INFINITY,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0xff;
	buffer[6] = 0xf0;
	buffer[5] = 0;
	buffer[4] = 0;
	buffer[3] = 0;
	buffer[2] = 0;
	buffer[1] = 0;
	buffer[0] = 0;
	ASSERT.equal(Number.NEGATIVE_INFINITY,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* Simple normalized values */

	/* +/- 1.125 */
	buffer[0] = 0x3f;
	buffer[1] = 0xf2;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 0;
	ASSERT.equal(1.125,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x3f;
	buffer[6] = 0xf2;
	buffer[5] = 0;
	buffer[4] = 0;
	buffer[3] = 0;
	buffer[2] = 0;
	buffer[1] = 0;
	buffer[0] = 0;
	ASSERT.equal(1.125,
	    mod_ctype.rdouble(buffer, 'little', 0));

	buffer[0] = 0xbf;
	buffer[1] = 0xf2;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 0;
	buffer[6] = 0;
	buffer[7] = 0;
	ASSERT.equal(-1.125,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0xbf;
	buffer[6] = 0xf2;
	buffer[5] = 0;
	buffer[4] = 0;
	buffer[3] = 0;
	buffer[2] = 0;
	buffer[1] = 0;
	buffer[0] = 0;
	ASSERT.equal(-1.125,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* +/- 1.4397318913736026e+283 */
	buffer[0] = 0x7a;
	buffer[1] = 0xb8;
	buffer[2] = 0xc9;
	buffer[3] = 0x34;
	buffer[4] = 0x72;
	buffer[5] = 0x16;
	buffer[6] = 0xf9;
	buffer[7] = 0x0e;
	ASSERT.equal(1.4397318913736026e+283,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x7a;
	buffer[6] = 0xb8;
	buffer[5] = 0xc9;
	buffer[4] = 0x34;
	buffer[3] = 0x72;
	buffer[2] = 0x16;
	buffer[1] = 0xf9;
	buffer[0] = 0x0e;
	ASSERT.equal(1.4397318913736026e+283,
	    mod_ctype.rdouble(buffer, 'little', 0));

	buffer[0] = 0xfa;
	buffer[1] = 0xb8;
	buffer[2] = 0xc9;
	buffer[3] = 0x34;
	buffer[4] = 0x72;
	buffer[5] = 0x16;
	buffer[6] = 0xf9;
	buffer[7] = 0x0e;
	ASSERT.equal(-1.4397318913736026e+283,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0xfa;
	buffer[6] = 0xb8;
	buffer[5] = 0xc9;
	buffer[4] = 0x34;
	buffer[3] = 0x72;
	buffer[2] = 0x16;
	buffer[1] = 0xf9;
	buffer[0] = 0x0e;
	ASSERT.equal(-1.4397318913736026e+283,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* Denormalized values */
	/* +/- 8.82521232268344e-309 */
	buffer[0] = 0x00;
	buffer[1] = 0x06;
	buffer[2] = 0x58;
	buffer[3] = 0x94;
	buffer[4] = 0x13;
	buffer[5] = 0x27;
	buffer[6] = 0x8a;
	buffer[7] = 0xcd;
	ASSERT.equal(8.82521232268344e-309,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x00;
	buffer[6] = 0x06;
	buffer[5] = 0x58;
	buffer[4] = 0x94;
	buffer[3] = 0x13;
	buffer[2] = 0x27;
	buffer[1] = 0x8a;
	buffer[0] = 0xcd;
	ASSERT.equal(8.82521232268344e-309,
	    mod_ctype.rdouble(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0x06;
	buffer[2] = 0x58;
	buffer[3] = 0x94;
	buffer[4] = 0x13;
	buffer[5] = 0x27;
	buffer[6] = 0x8a;
	buffer[7] = 0xcd;
	ASSERT.equal(-8.82521232268344e-309,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x80;
	buffer[6] = 0x06;
	buffer[5] = 0x58;
	buffer[4] = 0x94;
	buffer[3] = 0x13;
	buffer[2] = 0x27;
	buffer[1] = 0x8a;
	buffer[0] = 0xcd;
	ASSERT.equal(-8.82521232268344e-309,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* Edge cases, maximum and minimum values */

	/* Smallest denormalized value 5e-324 */
	buffer[0] = 0x00;
	buffer[1] = 0x00;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	buffer[4] = 0x00;
	buffer[5] = 0x00;
	buffer[6] = 0x00;
	buffer[7] = 0x01;
	ASSERT.equal(5e-324,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x00;
	buffer[6] = 0x00;
	buffer[5] = 0x00;
	buffer[4] = 0x00;
	buffer[3] = 0x00;
	buffer[2] = 0x00;
	buffer[1] = 0x00;
	buffer[0] = 0x01;
	ASSERT.equal(5e-324,
	    mod_ctype.rdouble(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0x00;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	buffer[4] = 0x00;
	buffer[5] = 0x00;
	buffer[6] = 0x00;
	buffer[7] = 0x01;
	ASSERT.equal(-5e-324,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x80;
	buffer[6] = 0x00;
	buffer[5] = 0x00;
	buffer[4] = 0x00;
	buffer[3] = 0x00;
	buffer[2] = 0x00;
	buffer[1] = 0x00;
	buffer[0] = 0x01;
	ASSERT.equal(-5e-324,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* Largest denormalized value 2.225073858507201e-308 */
	buffer[0] = 0x00;
	buffer[1] = 0x0f;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	buffer[4] = 0xff;
	buffer[5] = 0xff;
	buffer[6] = 0xff;
	buffer[7] = 0xff;
	ASSERT.equal(2.225073858507201e-308,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x00;
	buffer[6] = 0x0f;
	buffer[5] = 0xff;
	buffer[4] = 0xff;
	buffer[3] = 0xff;
	buffer[2] = 0xff;
	buffer[1] = 0xff;
	buffer[0] = 0xff;
	ASSERT.equal(2.225073858507201e-308,
	    mod_ctype.rdouble(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0x0f;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	buffer[4] = 0xff;
	buffer[5] = 0xff;
	buffer[6] = 0xff;
	buffer[7] = 0xff;
	ASSERT.equal(-2.225073858507201e-308,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x80;
	buffer[6] = 0x0f;
	buffer[5] = 0xff;
	buffer[4] = 0xff;
	buffer[3] = 0xff;
	buffer[2] = 0xff;
	buffer[1] = 0xff;
	buffer[0] = 0xff;
	ASSERT.equal(-2.225073858507201e-308,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* Smallest normalized value 2.2250738585072014e-308 */
	buffer[0] = 0x00;
	buffer[1] = 0x10;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	buffer[4] = 0x00;
	buffer[5] = 0x00;
	buffer[6] = 0x00;
	buffer[7] = 0x00;
	ASSERT.equal(2.2250738585072014e-308,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x00;
	buffer[6] = 0x10;
	buffer[5] = 0x00;
	buffer[4] = 0x00;
	buffer[3] = 0x00;
	buffer[2] = 0x00;
	buffer[1] = 0x00;
	buffer[0] = 0x00;
	ASSERT.equal(2.2250738585072014e-308,
	    mod_ctype.rdouble(buffer, 'little', 0));

	buffer[0] = 0x80;
	buffer[1] = 0x10;
	buffer[2] = 0x00;
	buffer[3] = 0x00;
	buffer[4] = 0x00;
	buffer[5] = 0x00;
	buffer[6] = 0x00;
	buffer[7] = 0x00;
	ASSERT.equal(-2.2250738585072014e-308,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x80;
	buffer[6] = 0x10;
	buffer[5] = 0x00;
	buffer[4] = 0x00;
	buffer[3] = 0x00;
	buffer[2] = 0x00;
	buffer[1] = 0x00;
	buffer[0] = 0x00;
	ASSERT.equal(-2.2250738585072014e-308,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* Largest normalized value 1.7976931348623157e+308 */
	buffer[0] = 0x7f;
	buffer[1] = 0xef;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	buffer[4] = 0xff;
	buffer[5] = 0xff;
	buffer[6] = 0xff;
	buffer[7] = 0xff;
	ASSERT.equal(1.7976931348623157e+308,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0x7f;
	buffer[6] = 0xef;
	buffer[5] = 0xff;
	buffer[4] = 0xff;
	buffer[3] = 0xff;
	buffer[2] = 0xff;
	buffer[1] = 0xff;
	buffer[0] = 0xff;
	ASSERT.equal(1.7976931348623157e+308,
	    mod_ctype.rdouble(buffer, 'little', 0));

	buffer[0] = 0xff;
	buffer[1] = 0xef;
	buffer[2] = 0xff;
	buffer[3] = 0xff;
	buffer[4] = 0xff;
	buffer[5] = 0xff;
	buffer[6] = 0xff;
	buffer[7] = 0xff;
	ASSERT.equal(-1.7976931348623157e+308,
	    mod_ctype.rdouble(buffer, 'big', 0));

	buffer[7] = 0xff;
	buffer[6] = 0xef;
	buffer[5] = 0xff;
	buffer[4] = 0xff;
	buffer[3] = 0xff;
	buffer[2] = 0xff;
	buffer[1] = 0xff;
	buffer[0] = 0xff;
	ASSERT.equal(-1.7976931348623157e+308,
	    mod_ctype.rdouble(buffer, 'little', 0));

	/* Try offsets */
	buffer[0] = 0xde;
	buffer[1] = 0xad;
	buffer[2] = 0xbe;
	buffer[3] = 0xef;
	buffer[4] = 0xba;
	buffer[5] = 0xdd;
	buffer[6] = 0xca;
	buffer[7] = 0xfe;
	buffer[8] = 0x16;
	buffer[9] = 0x79;

	ASSERT.equal(-1.1885958404126936e+148,
	    mod_ctype.rdouble(buffer, 'big', 0));
	ASSERT.equal(-2.4299184080448593e-88,
	    mod_ctype.rdouble(buffer, 'big', 1));
	ASSERT.equal(-0.000015130017658081283,
	    mod_ctype.rdouble(buffer, 'big', 2));

	ASSERT.equal(-5.757458694845505e+302,
	    mod_ctype.rdouble(buffer, 'little', 0));
	ASSERT.equal(6.436459604192476e-198,
	    mod_ctype.rdouble(buffer, 'little', 1));
	ASSERT.equal(1.9903745632417286e+275,
	    mod_ctype.rdouble(buffer, 'little', 2));
}

testfloat();
testdouble();
