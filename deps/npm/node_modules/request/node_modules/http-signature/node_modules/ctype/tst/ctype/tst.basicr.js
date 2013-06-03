/*
 * Simple does to see if it works at all
 */
var mod_ctype = require('../../ctype');
var ASSERT = require('assert');
var mod_sys = require('sys');

function test()
{
	var ii, p, result, buffer;

	p = new mod_ctype.Parser({ endian: 'little' });
	buffer = new Buffer(4);
	buffer[0] = 23;
	buffer[3] = 42;
	result = p.readData([ { x: { type: 'uint8_t' }},
	    { y: { type: 'uint8_t', offset: 3 }}
	], buffer, 0);
	ASSERT.equal(23, result['x']);
	ASSERT.equal(42, result['y']);

	buffer = new Buffer(23);
	for (ii = 0; ii < 23; ii++)
		buffer[ii] = 0;

	buffer.write('Hello, world!');
	result = p.readData([ { x: { type: 'char[20]' }} ], buffer, 0);

	/*
	 * This is currently broken behvaior, need to redesign check
	 * ASSERT.equal('Hello, world!', result['x'].toString('utf-8', 0,
	 *  result['x'].length));
	 */

	buffer = new Buffer(4);
	buffer[0] = 0x03;
	buffer[1] = 0x24;
	buffer[2] = 0x25;
	buffer[3] = 0x26;
	result = p.readData([ { y: { type: 'uint8_t' }},
	    { x: { type: 'uint8_t[y]' }}], buffer, 0);
	console.log(mod_sys.inspect(result, true));

	p.typedef('ssize_t', 'int32_t');
	ASSERT.deepEqual({ 'ssize_t': 'int32_t' }, p.lstypes());
	result = p.readData([ { x: { type: 'ssize_t' } } ], buffer, 0);
	ASSERT.equal(0x26252403, result['x']);
}

test();
