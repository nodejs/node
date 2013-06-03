/*
 * Test to verify that the offset is incremented when structures are written to.
 * Hopefully we will not regress issue #41
 */

var mod_ctype = require('../../ctype.js');
var mod_assert = require('assert');

function test()
{
	var parser, buf, data;
	parser = new mod_ctype.Parser({
	    endian: 'little'
	});
	parser.typedef('point_t', [
	    { x: { type: 'uint8_t' } },
	    { y: { type: 'uint8_t' } }
	]);
	buf = new Buffer(4);
	data = [
	    { point1: { type: 'point_t' } },
	    { point2: { type: 'point_t' } }
	];
	parser.writeData(data, buf, 0, [ [ 23, 42 ], [ 91, 18 ] ]);
	mod_assert.ok(buf[0] == 23);
	mod_assert.ok(buf[1] == 42);
	mod_assert.ok(buf[2] == 91);
	mod_assert.ok(buf[3] == 18);
}

test();
