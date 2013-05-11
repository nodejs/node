/*
 * Test to make sure that we properly are erroring whenever we try to write
 * beyond the size of the integer.
 */

var mod_ctio = require('../../../ctio.js');
var mod_assert = require('assert');
var tb = new Buffer(16); /* Largest buffer we'll need */

var cases = [
	{ func:
	function () {
		mod_ctio.wsint8(0x80, 'big', tb, 0);
	}, test: '+int8_t' },
	{ func:
	function () {
		mod_ctio.wsint8(-0x81, 'big', tb, 0);
	}, test: '-int8_t' },

	{ func:
	function () {
		mod_ctio.wsint16(0x8000, 'big', tb, 0);
	}, test: '+int16_t' },
	{ func:
	function () {
		mod_ctio.wsint16(-0x8001, 'big', tb, 0);
	}, test: '-int16_t' },
	{ func:
	function () {
		mod_ctio.wsint32(0x80000000, 'big', tb, 0);
	}, test: '+int32_t' },
	{ func:
	function () {
		mod_ctio.wsint32(-0x80000001, 'big', tb, 0);
	}, test: '-int32_t' },
	{ func:
	function () {
		mod_ctio.wsint64([ 0x80000000, 0 ], 'big', tb, 0);
	}, test: '+int64_t' },
	{ func:
	function () {
		mod_ctio.wsint64([ -0x80000000, -1 ], 'big', tb, 0);
	}, test: '-int64_t' }
];

function test()
{
	var ii;
	for (ii = 0; ii < cases.length; ii++)
		mod_assert.throws(cases[ii]['func'], Error, cases[ii]['test']);
}

test();
