'use strict';

require('../common');
const assert = require('assert');
const URL = require('url').URL;

const m = new URL('http://example.org');
let params = m.searchParams;

// Until we export URLSearchParams
const URLSearchParams = params.constructor;

let matches;

// getAll() basics
params = new URLSearchParams('a=b&c=d');
assert.deepStrictEqual(params.getAll('a'), ['b']);
assert.deepStrictEqual(params.getAll('c'), ['d']);
assert.deepStrictEqual(params.getAll('e'), []);
params = new URLSearchParams('a=b&c=d&a=e');
assert.deepStrictEqual(params.getAll('a'), ['b', 'e']);
params = new URLSearchParams('=b&c=d');
assert.deepStrictEqual(params.getAll(''), ['b']);
params = new URLSearchParams('a=&c=d&a=e');
assert.deepStrictEqual(params.getAll('a'), ['', 'e']);

// getAll() multiples
params = new URLSearchParams('a=1&a=2&a=3&a');
assert.strictEqual(true, params.has('a'),
                   'Search params object has name "a"');
matches = params.getAll('a');
assert(matches && matches.length == 4,
       'Search params object has values for name "a"');
assert.deepStrictEqual(matches, ['1', '2', '3', ''],
                       'Search params object has expected name "a" values');
params.set('a', 'one');
assert.strictEqual(params.get('a'), 'one',
                   'Search params object has name "a" with value "one"');
matches = params.getAll('a');
assert(matches && matches.length == 1,
       'Search params object has values for name "a"');
assert.deepStrictEqual(matches, ['one'],
                       'Search params object has expected name "a" values');
