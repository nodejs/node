'use strict';

const common = require('../common');
const assert = require('assert');
const { URL, URLSearchParams } = require('url');
const { test, assert_equals, assert_true, assert_false } = common.WPT;

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/urlsearchparams-delete.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
test(function() {
    var params = new URLSearchParams('a=b&c=d');
    params.delete('a');
    assert_equals(params + '', 'c=d');
    params = new URLSearchParams('a=a&b=b&a=a&c=c');
    params.delete('a');
    assert_equals(params + '', 'b=b&c=c');
    params = new URLSearchParams('a=a&=&b=b&c=c');
    params.delete('');
    assert_equals(params + '', 'a=a&b=b&c=c');
    params = new URLSearchParams('a=a&null=null&b=b');
    params.delete(null);
    assert_equals(params + '', 'a=a&b=b');
    params = new URLSearchParams('a=a&undefined=undefined&b=b');
    params.delete(undefined);
    assert_equals(params + '', 'a=a&b=b');
}, 'Delete basics');

test(function() {
    var params = new URLSearchParams();
    params.append('first', 1);
    assert_true(params.has('first'), 'Search params object has name "first"');
    assert_equals(params.get('first'), '1', 'Search params object has name "first" with value "1"');
    params.delete('first');
    assert_false(params.has('first'), 'Search params object has no "first" name');
    params.append('first', 1);
    params.append('first', 10);
    params.delete('first');
    assert_false(params.has('first'), 'Search params object has no "first" name');
}, 'Deleting appended multiple');
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.delete.call(undefined);
  }, /^TypeError: Value of `this` is not a URLSearchParams$/);
  assert.throws(() => {
    params.delete();
  }, /^TypeError: "name" argument must be specified$/);
}

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
