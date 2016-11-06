'use strict';

require('../common');
const assert = require('assert');
const URL = require('url').URL;

const m = new URL('http://example.org');
let params = m.searchParams;

// Until we export URLSearchParams
const URLSearchParams = params.constructor;

// Delete basics
params = new URLSearchParams('a=b&c=d');
params.delete('a');
assert.strictEqual(params + '', 'c=d');
params = new URLSearchParams('a=a&b=b&a=a&c=c');
params.delete('a');
assert.strictEqual(params + '', 'b=b&c=c');
params = new URLSearchParams('a=a&=&b=b&c=c');
params.delete('');
assert.strictEqual(params + '', 'a=a&b=b&c=c');
params = new URLSearchParams('a=a&null=null&b=b');
params.delete(null);
assert.strictEqual(params + '', 'a=a&b=b');
params = new URLSearchParams('a=a&undefined=undefined&b=b');
params.delete(undefined);
assert.strictEqual(params + '', 'a=a&b=b');

// Deleting appended multiple
params = new URLSearchParams();
params.append('first', 1);
assert.strictEqual(true, params.has('first'),
                   'Search params object has name "first"');
assert.strictEqual(params.get('first'), '1',
                   'Search params object has name "first" with value "1"');
params.delete('first');
assert.strictEqual(false, params.has('first'),
                   'Search params object has no "first" name');
params.append('first', 1);
params.append('first', 10);
params.delete('first');
assert.strictEqual(false, params.has('first'),
                   'Search params object has no "first" name');
