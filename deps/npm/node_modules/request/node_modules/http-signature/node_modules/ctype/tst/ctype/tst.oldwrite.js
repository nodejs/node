/*
 * A long overdue test to go through and verify that we can read and write
 * structures as well as nested structures.
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
	buf = new Buffer(2);
	data = [
	    { point: { type: 'point_t', value: [ 23, 42 ] } }
	];
	parser.writeData(data, buf, 0);
	mod_assert.ok(buf[0] == 23);
	mod_assert.ok(buf[1] == 42);
}

test();
