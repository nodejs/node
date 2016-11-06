'use strict';

require('../common');
const assert = require('assert');
const URL = require('url').URL;

const m = new URL('http://example.org');
let params = m.searchParams;

// Until we export URLSearchParams
const URLSearchParams = params.constructor;

// Has basics
params = new URLSearchParams('a=b&c=d');
assert.strictEqual(true, params.has('a'));
assert.strictEqual(true, params.has('c'));
assert.strictEqual(false, params.has('e'));
params = new URLSearchParams('a=b&c=d&a=e');
assert.strictEqual(true, params.has('a'));
params = new URLSearchParams('=b&c=d');
assert.strictEqual(true, params.has(''));
params = new URLSearchParams('null=a');
assert.strictEqual(true, params.has(null));

// has() following delete()
params = new URLSearchParams('a=b&c=d&&');
params.append('first', 1);
params.append('first', 2);
assert.strictEqual(true, params.has('a'),
                   'Search params object has name "a"');
assert.strictEqual(true, params.has('c'),
                   'Search params object has name "c"');
assert.strictEqual(true, params.has('first'),
                   'Search params object has name "first"');
assert.strictEqual(false, params.has('d'),
                   'Search params object has no name "d"');
params.delete('first');
assert.strictEqual(false, params.has('first'),
                   'Search params object has no name "first"');
