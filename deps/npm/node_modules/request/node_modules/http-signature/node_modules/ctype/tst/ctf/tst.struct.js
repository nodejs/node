var mod_fs = require('fs');
var mod_ctype = require('../../ctype.js');
var mod_assert = require('assert');

function test()
{
	var data, parser;

	data = JSON.parse(mod_fs.readFileSync('./struct.json').toString());
	parser = mod_ctype.parseCTF(data, { endian: 'big' });
	mod_assert.deepEqual(parser.lstypes(), { 'long': 'int32_t',
	    'time_t': 'long',
	    'timestruc_t': 'struct timespec',
	    'struct timespec': [ { 'tv_sec': { 'type': 'time_t' } },
	        { 'tv_nsec': { 'type': 'long' } } ] });
}

test();
