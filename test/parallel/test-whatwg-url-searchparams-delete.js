'use strict';

const common = require('../common');
const assert = require('assert');
const { URL, URLSearchParams } = require('url');
const { test, assert_equals, assert_true, assert_false } =
  require('../common/wpt');

/* The following tests are copied from WPT. Modifications to them should be
   upstreamed first. Refs:
   https://github.com/w3c/web-platform-tests/blob/70a0898763/url/urlsearchparams-delete.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
/* eslint-disable */
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

test(function() {
    var url = new URL('http://example.com/?param1&param2');
    url.searchParams.delete('param1');
    url.searchParams.delete('param2');
    assert_equals(url.href, 'http://example.com/', 'url.href does not have ?');
    assert_equals(url.search, '', 'url.search does not have ?');
}, 'Deleting all params removes ? from URL');

test(function() {
    var url = new URL('http://example.com/?');
    url.searchParams.delete('param1');
    assert_equals(url.href, 'http://example.com/', 'url.href does not have ?');
    assert_equals(url.search, '', 'url.search does not have ?');
}, 'Removing non-existent param removes ? from URL');
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.delete.call(undefined);
  }, common.expectsError({
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type URLSearchParams'
  }));
  assert.throws(() => {
    params.delete();
  }, common.expectsError({
    code: 'ERR_MISSING_ARGS',
    type: TypeError,
    message: 'The "name" argument must be specified'
  }));

  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  assert.throws(() => params.delete(obj), /^Error: toString$/);
  assert.throws(() => params.delete(sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
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
