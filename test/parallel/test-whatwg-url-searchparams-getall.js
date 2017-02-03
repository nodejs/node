'use strict';

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;
const { test, assert_equals, assert_true, assert_array_equals } = common.WPT;

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/urlsearchparams-getall.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
test(function() {
    var params = new URLSearchParams('a=b&c=d');
    assert_array_equals(params.getAll('a'), ['b']);
    assert_array_equals(params.getAll('c'), ['d']);
    assert_array_equals(params.getAll('e'), []);
    params = new URLSearchParams('a=b&c=d&a=e');
    assert_array_equals(params.getAll('a'), ['b', 'e']);
    params = new URLSearchParams('=b&c=d');
    assert_array_equals(params.getAll(''), ['b']);
    params = new URLSearchParams('a=&c=d&a=e');
    assert_array_equals(params.getAll('a'), ['', 'e']);
}, 'getAll() basics');

test(function() {
    var params = new URLSearchParams('a=1&a=2&a=3&a');
    assert_true(params.has('a'), 'Search params object doesn\'t have name "a"');
    var matches = params.getAll('a');
    assert(matches);
    assert_equals(matches.length, 4, `Unexpected length of name "a" values in 
      search params object: ${matches.length}`);
    assert_array_equals(matches, ['1', '2', '3', ''], `Unexpected name "a" 
      values: ${matches}`);
    params.set('a', 'one');
    assert_equals(params.get('a'), 'one', 'Search params object doesn\'t ' +
      'have name "a" with value "one"');
    matches = params.getAll('a');
    assert(matches);
    assert_equals(matches.length, 1, `Unexpected length of name "a" values in 
      search params object: ${matches.length}`);
    assert_array_equals(matches, ['one'], `Unexpected name "a" values: 
      ${matches}`);
}, 'getAll() multiples');
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.getAll.call(undefined);
  }, /^TypeError: Value of `this` is not a URLSearchParams$/);
  assert.throws(() => {
    params.getAll();
  }, /^TypeError: "name" argument must be specified$/);
}
