/*
 * Test several conditions that should always cause us to throw.
 */
var mod_assert = require('assert');
var mod_ctype = require('../../ctype.js');

var cases = [
{ json: { }, msg: 'bad JSON - no metadata or data' },
{ json: { metadata: {} }, msg: 'bad JSON - bad metadata section' },
{ json: { metadata: { 'JSON version': [] } },
    msg: 'bad JSON - bad JSON version' },
{ json: { metadata: { 'JSON version': 2 } },
    msg: 'bad JSON - bad JSON version' },
{ json: { metadata: { 'JSON version': '100.20' } },
    msg: 'bad JSON - bad JSON version' },
{ json: { metadata: { 'JSON version': '1.0' } },
    msg: 'missing data section' },
{ json: { metadata: { 'JSON version': '1.0' }, data: 1 },
    msg: 'invalid data section' },
{ json: { metadata: { 'JSON version': '1.0' }, data: 1.1 },
    msg: 'invalid data section' },
{ json: { metadata: { 'JSON version': '1.0' }, data: '1.1' },
    msg: 'invalid data section' },
{ json: { metadata: { 'JSON version': '1.0' }, data: {} },
    msg: 'invalid data section' }
];

function test()
{
	var ii;

	for (ii = 0; ii < cases.length; ii++) {
		mod_assert.throws(function () {
		    mod_ctype.parseCTF(cases[ii].json);
		}, Error, cases[ii].msg);
	}
}

test();
