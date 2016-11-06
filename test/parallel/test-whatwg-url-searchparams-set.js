'use strict';

require('../common');
const assert = require('assert');
const URL = require('url').URL;

const m = new URL('http://example.org');
let params = m.searchParams;

// Until we export URLSearchParams
const URLSearchParams = params.constructor;

// Set basics
params = new URLSearchParams('a=b&c=d');
params.set('a', 'B');
assert.strictEqual(params + '', 'a=B&c=d');
params = new URLSearchParams('a=b&c=d&a=e');
params.set('a', 'B');
assert.strictEqual(params + '', 'a=B&c=d');
params.set('e', 'f');
assert.strictEqual(params + '', 'a=B&c=d&e=f');

// URLSearchParams.set
params = new URLSearchParams('a=1&a=2&a=3');
assert.strictEqual(true, params.has('a'),
                   'Search params object has name "a"');
assert.strictEqual(params.get('a'), '1',
                   'Search params object has name "a" with value "1"');
params.set('first', 4);
assert.strictEqual(true, params.has('a'),
                   'Search params object has name "a"');
assert.strictEqual(params.get('a'), '1',
                   'Search params object has name "a" with value "1"');
params.set('a', 4);
assert.strictEqual(true, params.has('a'),
                   'Search params object has name "a"');
assert.strictEqual(params.get('a'), '4',
                   'Search params object has name "a" with value "4"');
