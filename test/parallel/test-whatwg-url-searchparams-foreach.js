'use strict';

const common = require('../common');
const assert = require('assert');
const { URL, URLSearchParams } = require('url');
const { test, assert_array_equals, assert_unreached } = common.WPT;

/* eslint-disable */
var i;  // Strict mode fix for WPT.
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/a8b2b1e/url/urlsearchparams-foreach.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
test(function() {
    var params = new URLSearchParams('a=1&b=2&c=3');
    var keys = [];
    var values = [];
    params.forEach(function(value, key) {
        keys.push(key);
        values.push(value);
    });
    assert_array_equals(keys, ['a', 'b', 'c']);
    assert_array_equals(values, ['1', '2', '3']);
}, "ForEach Check");

test(function() {
    let a = new URL("http://a.b/c?a=1&b=2&c=3&d=4");
    let b = a.searchParams;
    var c = [];
    for (i of b) {
        a.search = "x=1&y=2&z=3";
        c.push(i);
    }
    assert_array_equals(c[0], ["a","1"]);
    assert_array_equals(c[1], ["y","2"]);
    assert_array_equals(c[2], ["z","3"]);
}, "For-of Check");

test(function() {
    let a = new URL("http://a.b/c");
    let b = a.searchParams;
    for (i of b) {
        assert_unreached(i);
    }
}, "empty");
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.forEach.call(undefined);
  }, /^TypeError: Value of `this` is not a URLSearchParams$/);
}
