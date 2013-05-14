/*
 * Simple does it fucking work at all test
 */

var mod_ctype = require('../../ctype');
var ASSERT = require('assert');
var mod_sys = require('sys');

function test()
{
	var ii, p, buffer, buf2;

	p = new mod_ctype.Parser({ endian: 'big' });
	buffer = new Buffer(4);
	p.writeData([ { x: { type: 'uint8_t', value: 23 }},
	    { y: { type: 'uint8_t', offset: 3, value: 42 }}
	], buffer, 0);
	ASSERT.equal(23, buffer[0]);
	ASSERT.equal(42, buffer[3]);

	buffer = new Buffer(20);
	for (ii = 0; ii < 20; ii++)
		buffer[ii] = 0;

	buffer.write('Hello, world!');
	buf2 = new Buffer(22);
	p.writeData([ { x: { type: 'char[20]', value: buffer }} ], buf2, 0);
	for (ii = 0; ii < 20; ii++)
		ASSERT.equal(buffer[ii], buf2[ii]);
	/*
	 * This is currently broken behvaior, need to redesign check
	 * ASSERT.equal('Hello, world!', result['x'].toString('utf-8', 0,
	 *   result['x'].length));
	 */

	buffer = new Buffer(4);
	p.writeData([ { y: { type: 'uint8_t', value: 3 }},
	    { x: { type: 'uint8_t[y]', value: [ 0x24, 0x25, 0x26] }}],
	    buffer, 0);
	console.log(mod_sys.inspect(buffer));

	p.typedef('ssize_t', 'int32_t');
	ASSERT.deepEqual({ 'ssize_t': 'int32_t' }, p.lstypes());
}
