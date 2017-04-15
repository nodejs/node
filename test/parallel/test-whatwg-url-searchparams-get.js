'use strict';

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;
const { test, assert_equals, assert_true } = common.WPT;

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/urlsearchparams-get.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
test(function() {
    var params = new URLSearchParams('a=b&c=d');
    assert_equals(params.get('a'), 'b');
    assert_equals(params.get('c'), 'd');
    assert_equals(params.get('e'), null);
    params = new URLSearchParams('a=b&c=d&a=e');
    assert_equals(params.get('a'), 'b');
    params = new URLSearchParams('=b&c=d');
    assert_equals(params.get(''), 'b');
    params = new URLSearchParams('a=&c=d&a=e');
    assert_equals(params.get('a'), '');
}, 'Get basics');

test(function() {
    var params = new URLSearchParams('first=second&third&&');
    assert_true(params != null, 'constructor returned non-null value.');
    assert_true(params.has('first'), 'Search params object has name "first"');
    assert_equals(params.get('first'), 'second', 'Search params object has name "first" with value "second"');
    assert_equals(params.get('third'), '', 'Search params object has name "third" with the empty value.');
    assert_equals(params.get('fourth'), null, 'Search params object has no "fourth" name and value.');
}, 'More get() basics');
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.get.call(undefined);
  }, /^TypeError: Value of `this` is not a URLSearchParams$/);
  assert.throws(() => {
    params.get();
  }, /^TypeError: "name" argument must be specified$/);

  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  assert.throws(() => params.get(obj), /^Error: toString$/);
  assert.throws(() => params.get(sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
}
