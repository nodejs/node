'use strict';

require('../common');
const assert = require('assert');
const URL = require('url').URL;

const m = new URL('http://example.org');
let params = m.searchParams;

// Until we export URLSearchParams
const URLSearchParams = params.constructor;

// Append same name
params = new URLSearchParams();
params.append('a', 'b');
assert.strictEqual(params + '', 'a=b');
params.append('a', 'b');
assert.strictEqual(params + '', 'a=b&a=b');
params.append('a', 'c');
assert.strictEqual(params + '', 'a=b&a=b&a=c');

// Append empty strings
params = new URLSearchParams();
params.append('', '');
assert.strictEqual(params + '', '=');
params.append('', '');
assert.strictEqual(params + '', '=&=');

// Append null
params = new URLSearchParams();
params.append(null, null);
assert.strictEqual(params + '', 'null=null');
params.append(null, null);
assert.strictEqual(params + '', 'null=null&null=null');

// Append multiple
params = new URLSearchParams();
params.append('first', 1);
params.append('second', 2);
params.append('third', '');
params.append('first', 10);
assert.strictEqual(true, params.has('first'),
                   'Search params object has name "first"');
assert.strictEqual(params.get('first'), '1',
                   'Search params object has name "first" with value "1"');
assert.strictEqual(params.get('second'), '2',
                   'Search params object has name "second" with value "2"');
assert.strictEqual(params.get('third'), '',
                   'Search params object has name "third" with value ""');
params.append('first', 10);
assert.strictEqual(params.get('first'), '1',
                   'Search params object has name "first" with value "1"');
