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
    assert_true(params.has('a'), 'Search params object has name "a"');
    var matches = params.getAll('a');
    assert_true(matches && matches.length == 4, 'Search params object has values for name "a"');
    assert_array_equals(matches, ['1', '2', '3', ''], 'Search params object has expected name "a" values');
    params.set('a', 'one');
    assert_equals(params.get('a'), 'one', 'Search params object has name "a" with value "one"');
    var matches = params.getAll('a');
    assert_true(matches && matches.length == 1, 'Search params object has values for name "a"');
    assert_array_equals(matches, ['one'], 'Search params object has expected name "a" values');
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
