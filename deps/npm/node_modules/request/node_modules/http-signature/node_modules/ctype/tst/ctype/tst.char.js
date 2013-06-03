/*
 * Test the different forms of reading characters:
 *
 *  - the default, a single element buffer
 *  - uint8, values are uint8_ts
 *  - int8, values are int8_ts
 */
var mod_ctype = require('../../ctype');
var mod_assert = require('assert');

function test()
{
	var p, buf, res;

	buf = new Buffer(1);
	buf[0] = 255;

	p = new mod_ctype.Parser({ endian: 'little'});
	res = p.readData([ { c: { type: 'char' }} ], buf, 0);
	res = res['c'];
	mod_assert.ok(res instanceof Buffer);
	mod_assert.equal(255, res[0]);

	p = new mod_ctype.Parser({ endian: 'little',
	    'char-type': 'int8' });
	res = p.readData([ { c: { type: 'char' }} ], buf, 0);
	res = res['c'];
	mod_assert.ok(typeof (res) == 'number', 'got typeof (res): ' +
	    typeof (res));
	mod_assert.equal(-1, res);

	p = new mod_ctype.Parser({ endian: 'little',
	    'char-type': 'uint8' });
	res = p.readData([ { c: { type: 'char' }} ], buf, 0);
	res = res['c'];
	mod_assert.ok(typeof (res) == 'number', 'got typeof (res): ' +
	    typeof (res));
	mod_assert.equal(255, res);

}

test();
