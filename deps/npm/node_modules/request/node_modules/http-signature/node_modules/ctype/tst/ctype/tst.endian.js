/*
 * Simple test to make sure that the endian setting works.
 */

var mod_ctype = require('../../ctype.js');
var mod_assert = require('assert');

function test()
{
	var parser, buf;

	parser = new mod_ctype.Parser({
	    endian: 'little'
	});

	buf = new Buffer(2);
	parser.writeData([ { key: { type: 'uint16_t' } } ], buf, 0, [ 0x1234 ]);
	mod_assert.equal(buf[0], 0x34);
	mod_assert.equal(buf[1], 0x12);
	parser.setEndian('big');

	parser.writeData([ { key: { type: 'uint16_t' } } ], buf, 0, [ 0x1234 ]);
	mod_assert.equal(buf[0], 0x12);
	mod_assert.equal(buf[1], 0x34);

	parser.setEndian('little');
	parser.writeData([ { key: { type: 'uint16_t' } } ], buf, 0, [ 0x1234 ]);
	mod_assert.equal(buf[0], 0x34);
	mod_assert.equal(buf[1], 0x12);
}

function fail()
{
	var parser;

	parser = new mod_ctype.Parser({
	    endian: 'little'
	});
	mod_assert.throws(function () {
		parser.setEndian('littlebigwrong');
	});
}

test();
fail();
