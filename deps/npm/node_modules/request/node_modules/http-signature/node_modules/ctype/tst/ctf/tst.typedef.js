var mod_fs = require('fs');
var mod_ctype = require('../../ctype.js');
var mod_assert = require('assert');

function test()
{
	var data, parser;

	data = JSON.parse(mod_fs.readFileSync('./typedef.json').toString());
	parser = mod_ctype.parseCTF(data, { endian: 'big' });
	mod_assert.deepEqual(parser.lstypes(), { 'bar_t': 'int',
	    'int': 'int32_t' });
}

test();
