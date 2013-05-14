/*
 * Another place to find bugs that may yet plague us. This time with writing out
 * floats to arrays. We are lazy and did basically just take the opposite of our
 * test code to read in values.
 */

var mod_ctype = require('../../../ctio.js');
var ASSERT = require('assert');


/*
 *	A useful thing to keep around for debugging
 *	console.log('buffer[0]: ' + buffer[0].toString(16));
 *	console.log('buffer[1]: ' + buffer[1].toString(16));
 *	console.log('buffer[2]: ' + buffer[2].toString(16));
 *	console.log('buffer[3]: ' + buffer[3].toString(16));
 *	console.log('buffer[4]: ' + buffer[4].toString(16));
 *	console.log('buffer[5]: ' + buffer[5].toString(16));
 * 	console.log('buffer[6]: ' + buffer[6].toString(16));
 *	console.log('buffer[7]: ' + buffer[7].toString(16));
 */

function testfloat()
{
	var buffer = new Buffer(4);
	mod_ctype.wfloat(0, 'big', buffer, 0);
	/* Start off with some of the easy ones: +zero */
	ASSERT.equal(0, buffer[0]);
	ASSERT.equal(0, buffer[1]);
	ASSERT.equal(0, buffer[2]);
	ASSERT.equal(0, buffer[3]);
	mod_ctype.wfloat(0, 'little', buffer, 0);
	ASSERT.equal(0, buffer[0]);
	ASSERT.equal(0, buffer[1]);
	ASSERT.equal(0, buffer[2]);
	ASSERT.equal(0, buffer[3]);

	/* Catch +infin */
	mod_ctype.wfloat(Number.POSITIVE_INFINITY, 'big', buffer, 0);
	ASSERT.equal(0x7f, buffer[0]);
	ASSERT.equal(0x80, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	mod_ctype.wfloat(Number.POSITIVE_INFINITY, 'little', buffer, 0);
	ASSERT.equal(0x7f, buffer[3]);
	ASSERT.equal(0x80, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	/* Catch -infin */
	mod_ctype.wfloat(Number.NEGATIVE_INFINITY, 'big', buffer, 0);
	ASSERT.equal(0xff, buffer[0]);
	ASSERT.equal(0x80, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	mod_ctype.wfloat(Number.NEGATIVE_INFINITY, 'little', buffer, 0);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0x80, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	/* Catch NaN */

	/*
	 * NaN Is a litle weird in its requirements, so we're going to encode a
	 * bit of how we actually implement it into this test. Probably not the
	 * best, since technically the sign is a don't care and the mantissa
	 * needs to just be non-zero.
	 */
	mod_ctype.wfloat(NaN, 'big', buffer, 0);
	ASSERT.equal(0x7f, buffer[0]);
	ASSERT.equal(0x80, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x17, buffer[3]);
	mod_ctype.wfloat(NaN, 'little', buffer, 0);
	ASSERT.equal(0x7f, buffer[3]);
	ASSERT.equal(0x80, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x17, buffer[0]);

	/* On to some basic tests */
	/* 1.125 */
	mod_ctype.wfloat(1.125, 'big', buffer, 0);
	ASSERT.equal(0x3f, buffer[0]);
	ASSERT.equal(0x90, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	mod_ctype.wfloat(1.125, 'little', buffer, 0);
	ASSERT.equal(0x3f, buffer[3]);
	ASSERT.equal(0x90, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	mod_ctype.wfloat(1.0000001192092896, 'big', buffer, 0);
	ASSERT.equal(0x3f, buffer[0]);
	ASSERT.equal(0x80, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x01, buffer[3]);
	mod_ctype.wfloat(1.0000001192092896, 'little', buffer, 0);
	ASSERT.equal(0x3f, buffer[3]);
	ASSERT.equal(0x80, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x01, buffer[0]);

	mod_ctype.wfloat(1.0000001192092896, 'big', buffer, 0);
	ASSERT.equal(0x3f, buffer[0]);
	ASSERT.equal(0x80, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x01, buffer[3]);
	mod_ctype.wfloat(1.0000001192092896, 'little', buffer, 0);
	ASSERT.equal(0x3f, buffer[3]);
	ASSERT.equal(0x80, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x01, buffer[0]);

	mod_ctype.wfloat(2.3283067140944524e-10, 'big', buffer, 0);
	ASSERT.equal(0x2f, buffer[0]);
	ASSERT.equal(0x80, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x01, buffer[3]);
	mod_ctype.wfloat(2.3283067140944524e-10, 'little', buffer, 0);
	ASSERT.equal(0x2f, buffer[3]);
	ASSERT.equal(0x80, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x01, buffer[0]);

	/* ff34a2b0 -2.4010576103645774e+38 */
	mod_ctype.wfloat(-2.4010576103645774e+38,
	    'big', buffer, 0);
	ASSERT.equal(0xff, buffer[0]);
	ASSERT.equal(0x34, buffer[1]);
	ASSERT.equal(0xa2, buffer[2]);
	ASSERT.equal(0xb0, buffer[3]);
	mod_ctype.wfloat(-2.4010576103645774e+38,
	    'little', buffer, 0);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0x34, buffer[2]);
	ASSERT.equal(0xa2, buffer[1]);
	ASSERT.equal(0xb0, buffer[0]);

	/* Denormalized tests */

	/* 0003f89a +/- 3.6468792534053364e-40 */
	mod_ctype.wfloat(3.6468792534053364e-40,
	    'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x03, buffer[1]);
	ASSERT.equal(0xf8, buffer[2]);
	ASSERT.equal(0x9a, buffer[3]);
	mod_ctype.wfloat(3.6468792534053364e-40,
	    'little', buffer, 0);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x03, buffer[2]);
	ASSERT.equal(0xf8, buffer[1]);
	ASSERT.equal(0x9a, buffer[0]);

	mod_ctype.wfloat(-3.6468792534053364e-40,
	    'big', buffer, 0);
	ASSERT.equal(0x80, buffer[0]);
	ASSERT.equal(0x03, buffer[1]);
	ASSERT.equal(0xf8, buffer[2]);
	ASSERT.equal(0x9a, buffer[3]);
	mod_ctype.wfloat(-3.6468792534053364e-40,
	    'little', buffer, 0);
	ASSERT.equal(0x80, buffer[3]);
	ASSERT.equal(0x03, buffer[2]);
	ASSERT.equal(0xf8, buffer[1]);
	ASSERT.equal(0x9a, buffer[0]);

	/* Maximum and minimum normalized and denormalized values */

	/* Largest normalized number +/- 3.4028234663852886e+38 */

	mod_ctype.wfloat(3.4028234663852886e+38,
	    'big', buffer, 0);
	ASSERT.equal(0x7f, buffer[0]);
	ASSERT.equal(0x7f, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);
	mod_ctype.wfloat(3.4028234663852886e+38,
	    'little', buffer, 0);
	ASSERT.equal(0x7f, buffer[3]);
	ASSERT.equal(0x7f, buffer[2]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[0]);

	mod_ctype.wfloat(-3.4028234663852886e+38,
	    'big', buffer, 0);
	ASSERT.equal(0xff, buffer[0]);
	ASSERT.equal(0x7f, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);
	mod_ctype.wfloat(-3.4028234663852886e+38,
	    'little', buffer, 0);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0x7f, buffer[2]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[0]);

	/* Smallest normalied number +/- 1.1754943508222875e-38 */

	mod_ctype.wfloat(1.1754943508222875e-38,
	    'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x80, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	mod_ctype.wfloat(1.1754943508222875e-38,
	    'little', buffer, 0);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x80, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	mod_ctype.wfloat(-1.1754943508222875e-38,
	    'big', buffer, 0);
	ASSERT.equal(0x80, buffer[0]);
	ASSERT.equal(0x80, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	mod_ctype.wfloat(-1.1754943508222875e-38,
	    'little', buffer, 0);
	ASSERT.equal(0x80, buffer[3]);
	ASSERT.equal(0x80, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	/* Smallest denormalized number 1.401298464324817e-45 */
	mod_ctype.wfloat(1.401298464324817e-45,
	    'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x01, buffer[3]);
	mod_ctype.wfloat(1.401298464324817e-45,
	    'little', buffer, 0);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x01, buffer[0]);

	mod_ctype.wfloat(-1.401298464324817e-45,
	    'big', buffer, 0);
	ASSERT.equal(0x80, buffer[0]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x01, buffer[3]);
	mod_ctype.wfloat(-1.401298464324817e-45,
	    'little', buffer, 0);
	ASSERT.equal(0x80, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x01, buffer[0]);

	/* Largest denormalized value +/- 1.1754942106924411e-38 */

	mod_ctype.wfloat(1.1754942106924411e-38,
	    'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x7f, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);
	mod_ctype.wfloat(1.1754942106924411e-38,
	    'little', buffer, 0);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x7f, buffer[2]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[0]);

	mod_ctype.wfloat(-1.1754942106924411e-38,
	    'big', buffer, 0);
	ASSERT.equal(0x80, buffer[0]);
	ASSERT.equal(0x7f, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);
	mod_ctype.wfloat(-1.1754942106924411e-38,
	    'little', buffer, 0);
	ASSERT.equal(0x80, buffer[3]);
	ASSERT.equal(0x7f, buffer[2]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[0]);

	/* Do some quick offset testing */
	buffer = new Buffer(6);
	mod_ctype.wfloat(-1.2027516403607578e-32,
	    'big', buffer, 2);
	ASSERT.equal(0x8a, buffer[2]);
	ASSERT.equal(0x79, buffer[3]);
	ASSERT.equal(0xcd, buffer[4]);
	ASSERT.equal(0x3f, buffer[5]);

	mod_ctype.wfloat(-1.2027516403607578e-32,
	    'little', buffer, 2);
	ASSERT.equal(0x8a, buffer[5]);
	ASSERT.equal(0x79, buffer[4]);
	ASSERT.equal(0xcd, buffer[3]);
	ASSERT.equal(0x3f, buffer[2]);

}

function testdouble()
{
	var buffer = new Buffer(10);

	/* Check 0 */
	mod_ctype.wdouble(0, 'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[7]);
	mod_ctype.wdouble(0, 'little', buffer, 0);
	ASSERT.equal(0x00, buffer[7]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	/* Check NaN */
	/* Similar to floats we are only generating a subset of values */
	mod_ctype.wdouble(NaN, 'big', buffer, 0);
	ASSERT.equal(0x7f, buffer[0]);
	ASSERT.equal(0xf0, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x17, buffer[7]);
	mod_ctype.wdouble(NaN, 'little', buffer, 0);
	ASSERT.equal(0x7f, buffer[7]);
	ASSERT.equal(0xf0, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x17, buffer[0]);

	/* pos inf */
	mod_ctype.wdouble(Number.POSITIVE_INFINITY,
	    'big', buffer, 0);
	ASSERT.equal(0x7f, buffer[0]);
	ASSERT.equal(0xf0, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[7]);
	mod_ctype.wdouble(Number.POSITIVE_INFINITY,
	    'little', buffer, 0);
	ASSERT.equal(0x7f, buffer[7]);
	ASSERT.equal(0xf0, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	/* neg inf */
	mod_ctype.wdouble(Number.NEGATIVE_INFINITY,
	    'big', buffer, 0);
	ASSERT.equal(0xff, buffer[0]);
	ASSERT.equal(0xf0, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[7]);
	mod_ctype.wdouble(Number.NEGATIVE_INFINITY,
	    'little', buffer, 0);
	ASSERT.equal(0xff, buffer[7]);
	ASSERT.equal(0xf0, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	/* Simple normalized values */

	/* +/- 1.125 */
	mod_ctype.wdouble(1.125,
	    'big', buffer, 0);
	ASSERT.equal(0x3f, buffer[0]);
	ASSERT.equal(0xf2, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[7]);

	mod_ctype.wdouble(1.125,
	    'little', buffer, 0);
	ASSERT.equal(0x3f, buffer[7]);
	ASSERT.equal(0xf2, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	mod_ctype.wdouble(-1.125,
	    'big', buffer, 0);
	ASSERT.equal(0xbf, buffer[0]);
	ASSERT.equal(0xf2, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[7]);

	mod_ctype.wdouble(-1.125,
	    'little', buffer, 0);
	ASSERT.equal(0xbf, buffer[7]);
	ASSERT.equal(0xf2, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);


	/* +/- 1.4397318913736026e+283 */
	mod_ctype.wdouble(1.4397318913736026e+283,
	    'big', buffer, 0);
	ASSERT.equal(0x7a, buffer[0]);
	ASSERT.equal(0xb8, buffer[1]);
	ASSERT.equal(0xc9, buffer[2]);
	ASSERT.equal(0x34, buffer[3]);
	ASSERT.equal(0x72, buffer[4]);
	ASSERT.equal(0x16, buffer[5]);
	ASSERT.equal(0xf9, buffer[6]);
	ASSERT.equal(0x0e, buffer[7]);

	mod_ctype.wdouble(1.4397318913736026e+283,
	    'little', buffer, 0);
	ASSERT.equal(0x7a, buffer[7]);
	ASSERT.equal(0xb8, buffer[6]);
	ASSERT.equal(0xc9, buffer[5]);
	ASSERT.equal(0x34, buffer[4]);
	ASSERT.equal(0x72, buffer[3]);
	ASSERT.equal(0x16, buffer[2]);
	ASSERT.equal(0xf9, buffer[1]);
	ASSERT.equal(0x0e, buffer[0]);

	mod_ctype.wdouble(-1.4397318913736026e+283,
	    'big', buffer, 0);
	ASSERT.equal(0xfa, buffer[0]);
	ASSERT.equal(0xb8, buffer[1]);
	ASSERT.equal(0xc9, buffer[2]);
	ASSERT.equal(0x34, buffer[3]);
	ASSERT.equal(0x72, buffer[4]);
	ASSERT.equal(0x16, buffer[5]);
	ASSERT.equal(0xf9, buffer[6]);
	ASSERT.equal(0x0e, buffer[7]);

	mod_ctype.wdouble(-1.4397318913736026e+283,
	    'little', buffer, 0);
	ASSERT.equal(0xfa, buffer[7]);
	ASSERT.equal(0xb8, buffer[6]);
	ASSERT.equal(0xc9, buffer[5]);
	ASSERT.equal(0x34, buffer[4]);
	ASSERT.equal(0x72, buffer[3]);
	ASSERT.equal(0x16, buffer[2]);
	ASSERT.equal(0xf9, buffer[1]);
	ASSERT.equal(0x0e, buffer[0]);

	/* Denormalized values */
	/* +/- 8.82521232268344e-309 */
	mod_ctype.wdouble(8.82521232268344e-309,
	    'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x06, buffer[1]);
	ASSERT.equal(0x58, buffer[2]);
	ASSERT.equal(0x94, buffer[3]);
	ASSERT.equal(0x13, buffer[4]);
	ASSERT.equal(0x27, buffer[5]);
	ASSERT.equal(0x8a, buffer[6]);
	ASSERT.equal(0xcd, buffer[7]);

	mod_ctype.wdouble(8.82521232268344e-309,
	    'little', buffer, 0);
	ASSERT.equal(0x00, buffer[7]);
	ASSERT.equal(0x06, buffer[6]);
	ASSERT.equal(0x58, buffer[5]);
	ASSERT.equal(0x94, buffer[4]);
	ASSERT.equal(0x13, buffer[3]);
	ASSERT.equal(0x27, buffer[2]);
	ASSERT.equal(0x8a, buffer[1]);
	ASSERT.equal(0xcd, buffer[0]);

	mod_ctype.wdouble(-8.82521232268344e-309,
	    'big', buffer, 0);
	ASSERT.equal(0x80, buffer[0]);
	ASSERT.equal(0x06, buffer[1]);
	ASSERT.equal(0x58, buffer[2]);
	ASSERT.equal(0x94, buffer[3]);
	ASSERT.equal(0x13, buffer[4]);
	ASSERT.equal(0x27, buffer[5]);
	ASSERT.equal(0x8a, buffer[6]);
	ASSERT.equal(0xcd, buffer[7]);

	mod_ctype.wdouble(-8.82521232268344e-309,
	    'little', buffer, 0);
	ASSERT.equal(0x80, buffer[7]);
	ASSERT.equal(0x06, buffer[6]);
	ASSERT.equal(0x58, buffer[5]);
	ASSERT.equal(0x94, buffer[4]);
	ASSERT.equal(0x13, buffer[3]);
	ASSERT.equal(0x27, buffer[2]);
	ASSERT.equal(0x8a, buffer[1]);
	ASSERT.equal(0xcd, buffer[0]);


	/* Edge cases, maximum and minimum values */

	/* Smallest denormalized value 5e-324 */
	mod_ctype.wdouble(5e-324,
	    'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x01, buffer[7]);

	mod_ctype.wdouble(5e-324,
	    'little', buffer, 0);
	ASSERT.equal(0x00, buffer[7]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x01, buffer[0]);

	mod_ctype.wdouble(-5e-324,
	    'big', buffer, 0);
	ASSERT.equal(0x80, buffer[0]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x01, buffer[7]);

	mod_ctype.wdouble(-5e-324,
	    'little', buffer, 0);
	ASSERT.equal(0x80, buffer[7]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x01, buffer[0]);



	/* Largest denormalized value 2.225073858507201e-308 */
	mod_ctype.wdouble(2.225073858507201e-308,
	    'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x0f, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0xff, buffer[4]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[6]);
	ASSERT.equal(0xff, buffer[7]);

	mod_ctype.wdouble(2.225073858507201e-308,
	    'little', buffer, 0);
	ASSERT.equal(0x00, buffer[7]);
	ASSERT.equal(0x0f, buffer[6]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[4]);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[0]);

	mod_ctype.wdouble(-2.225073858507201e-308,
	    'big', buffer, 0);
	ASSERT.equal(0x80, buffer[0]);
	ASSERT.equal(0x0f, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0xff, buffer[4]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[6]);
	ASSERT.equal(0xff, buffer[7]);

	mod_ctype.wdouble(-2.225073858507201e-308,
	    'little', buffer, 0);
	ASSERT.equal(0x80, buffer[7]);
	ASSERT.equal(0x0f, buffer[6]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[4]);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[0]);


	/* Smallest normalized value 2.2250738585072014e-308 */
	mod_ctype.wdouble(2.2250738585072014e-308,
	    'big', buffer, 0);
	ASSERT.equal(0x00, buffer[0]);
	ASSERT.equal(0x10, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[7]);

	mod_ctype.wdouble(2.2250738585072014e-308,
	    'little', buffer, 0);
	ASSERT.equal(0x00, buffer[7]);
	ASSERT.equal(0x10, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);

	mod_ctype.wdouble(-2.2250738585072014e-308,
	    'big', buffer, 0);
	ASSERT.equal(0x80, buffer[0]);
	ASSERT.equal(0x10, buffer[1]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[6]);
	ASSERT.equal(0x00, buffer[7]);

	mod_ctype.wdouble(-2.2250738585072014e-308,
	    'little', buffer, 0);
	ASSERT.equal(0x80, buffer[7]);
	ASSERT.equal(0x10, buffer[6]);
	ASSERT.equal(0x00, buffer[5]);
	ASSERT.equal(0x00, buffer[4]);
	ASSERT.equal(0x00, buffer[3]);
	ASSERT.equal(0x00, buffer[2]);
	ASSERT.equal(0x00, buffer[1]);
	ASSERT.equal(0x00, buffer[0]);


	/* Largest normalized value 1.7976931348623157e+308 */
	mod_ctype.wdouble(1.7976931348623157e+308,
	    'big', buffer, 0);
	ASSERT.equal(0x7f, buffer[0]);
	ASSERT.equal(0xef, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0xff, buffer[4]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[6]);
	ASSERT.equal(0xff, buffer[7]);

	mod_ctype.wdouble(1.7976931348623157e+308,
	    'little', buffer, 0);
	ASSERT.equal(0x7f, buffer[7]);
	ASSERT.equal(0xef, buffer[6]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[4]);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[0]);

	mod_ctype.wdouble(-1.7976931348623157e+308,
	    'big', buffer, 0);
	ASSERT.equal(0xff, buffer[0]);
	ASSERT.equal(0xef, buffer[1]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0xff, buffer[4]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[6]);
	ASSERT.equal(0xff, buffer[7]);

	mod_ctype.wdouble(-1.7976931348623157e+308,
	    'little', buffer, 0);
	ASSERT.equal(0xff, buffer[7]);
	ASSERT.equal(0xef, buffer[6]);
	ASSERT.equal(0xff, buffer[5]);
	ASSERT.equal(0xff, buffer[4]);
	ASSERT.equal(0xff, buffer[3]);
	ASSERT.equal(0xff, buffer[2]);
	ASSERT.equal(0xff, buffer[1]);
	ASSERT.equal(0xff, buffer[0]);


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

	mod_ctype.wdouble(-0.000015130017658081283,
	    'big', buffer, 2);
	ASSERT.equal(0xbe, buffer[2]);
	ASSERT.equal(0xef, buffer[3]);
	ASSERT.equal(0xba, buffer[4]);
	ASSERT.equal(0xdd, buffer[5]);
	ASSERT.equal(0xca, buffer[6]);
	ASSERT.equal(0xfe, buffer[7]);
	ASSERT.equal(0x16, buffer[8]);
	ASSERT.equal(0x79, buffer[9]);

	mod_ctype.wdouble(-0.000015130017658081283,
	    'little', buffer, 2);
	ASSERT.equal(0xbe, buffer[9]);
	ASSERT.equal(0xef, buffer[8]);
	ASSERT.equal(0xba, buffer[7]);
	ASSERT.equal(0xdd, buffer[6]);
	ASSERT.equal(0xca, buffer[5]);
	ASSERT.equal(0xfe, buffer[4]);
	ASSERT.equal(0x16, buffer[3]);
	ASSERT.equal(0x79, buffer[2]);
}

testfloat();
testdouble();
