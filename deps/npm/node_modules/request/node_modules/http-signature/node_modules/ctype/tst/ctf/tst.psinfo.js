var mod_fs = require('fs');
var mod_ctype = require('../../ctype.js');
var mod_assert = require('assert');

/*
 * This is too unwieldly to actually write out. Just make sure we can parse it
 * without errrors.
 */
function test()
{
	var data;

	data = JSON.parse(mod_fs.readFileSync('./psinfo.json').toString());
	mod_ctype.parseCTF(data, { endian: 'big' });
}

test();
