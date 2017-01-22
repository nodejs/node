'use strict';

require('../common');
const assert = require('assert');
const url = require('url');
const URL = url.URL;
const URLSearchParams = url.URLSearchParams;

let params;

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

assert.throws(() => {
  params.delete.call(undefined);
}, /^TypeError: Value of `this` is not a URLSearchParams$/);
assert.throws(() => {
  params.delete();
}, /^TypeError: "name" argument must be specified$/);

// https://github.com/nodejs/node/issues/10480
// Emptying searchParams should correctly update url's query
{
  const url = new URL('http://domain?var=1&var=2&var=3');
  for (const param of url.searchParams.keys()) {
    url.searchParams.delete(param);
  }
  assert.strictEqual(url.searchParams.toString(), '');
  assert.strictEqual(url.search, '');
  assert.strictEqual(url.href, 'http://domain/');
}
