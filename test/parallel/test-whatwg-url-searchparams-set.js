'use strict';

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;
const { test, assert_equals, assert_true } = common.WPT;

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/urlsearchparams-set.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
test(function() {
    var params = new URLSearchParams('a=b&c=d');
    params.set('a', 'B');
    assert_equals(params + '', 'a=B&c=d');
    params = new URLSearchParams('a=b&c=d&a=e');
    params.set('a', 'B');
    assert_equals(params + '', 'a=B&c=d')
    params.set('e', 'f');
    assert_equals(params + '', 'a=B&c=d&e=f')
}, 'Set basics');

test(function() {
    var params = new URLSearchParams('a=1&a=2&a=3');
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_equals(params.get('a'), '1', 'Search params object has name "a" with value "1"');
    params.set('first', 4);
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_equals(params.get('a'), '1', 'Search params object has name "a" with value "1"');
    params.set('a', 4);
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_equals(params.get('a'), '4', 'Search params object has name "a" with value "4"');
}, 'URLSearchParams.set');
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.set.call(undefined);
  }, /^TypeError: Value of `this` is not a URLSearchParams$/);
  assert.throws(() => {
    params.set('a');
  }, /^TypeError: "name" and "value" arguments must be specified$/);
}
