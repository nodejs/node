'use strict';

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;
const { test, assert_equals } = common.WPT;

/* eslint-disable */
/* WPT Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/urlsearchparams-stringifier.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
test(function() {
    var params = new URLSearchParams();
    params.append('a', 'b c');
    assert_equals(params + '', 'a=b+c');
    params.delete('a');
    params.append('a b', 'c');
    assert_equals(params + '', 'a+b=c');
}, 'Serialize space');

test(function() {
    var params = new URLSearchParams();
    params.append('a', '');
    assert_equals(params + '', 'a=');
    params.append('a', '');
    assert_equals(params + '', 'a=&a=');
    params.append('', 'b');
    assert_equals(params + '', 'a=&a=&=b');
    params.append('', '');
    assert_equals(params + '', 'a=&a=&=b&=');
    params.append('', '');
    assert_equals(params + '', 'a=&a=&=b&=&=');
}, 'Serialize empty value');

test(function() {
    var params = new URLSearchParams();
    params.append('', 'b');
    assert_equals(params + '', '=b');
    params.append('', 'b');
    assert_equals(params + '', '=b&=b');
}, 'Serialize empty name');

test(function() {
    var params = new URLSearchParams();
    params.append('', '');
    assert_equals(params + '', '=');
    params.append('', '');
    assert_equals(params + '', '=&=');
}, 'Serialize empty name and value');

test(function() {
    var params = new URLSearchParams();
    params.append('a', 'b+c');
    assert_equals(params + '', 'a=b%2Bc');
    params.delete('a');
    params.append('a+b', 'c');
    assert_equals(params + '', 'a%2Bb=c');
}, 'Serialize +');

test(function() {
    var params = new URLSearchParams();
    params.append('=', 'a');
    assert_equals(params + '', '%3D=a');
    params.append('b', '=');
    assert_equals(params + '', '%3D=a&b=%3D');
}, 'Serialize =');

test(function() {
    var params = new URLSearchParams();
    params.append('&', 'a');
    assert_equals(params + '', '%26=a');
    params.append('b', '&');
    assert_equals(params + '', '%26=a&b=%26');
}, 'Serialize &');

test(function() {
    var params = new URLSearchParams();
    params.append('a', '*-._');
    assert_equals(params + '', 'a=*-._');
    params.delete('a');
    params.append('*-._', 'c');
    assert_equals(params + '', '*-._=c');
}, 'Serialize *-._');

test(function() {
    var params = new URLSearchParams();
    params.append('a', 'b%c');
    assert_equals(params + '', 'a=b%25c');
    params.delete('a');
    params.append('a%b', 'c');
    assert_equals(params + '', 'a%25b=c');
}, 'Serialize %');

test(function() {
    var params = new URLSearchParams();
    params.append('a', 'b\0c');
    assert_equals(params + '', 'a=b%00c');
    params.delete('a');
    params.append('a\0b', 'c');
    assert_equals(params + '', 'a%00b=c');
}, 'Serialize \\0');

test(function() {
    var params = new URLSearchParams();
    params.append('a', 'b\uD83D\uDCA9c');
    assert_equals(params + '', 'a=b%F0%9F%92%A9c');
    params.delete('a');
    params.append('a\uD83D\uDCA9b', 'c');
    assert_equals(params + '', 'a%F0%9F%92%A9b=c');
}, 'Serialize \uD83D\uDCA9');  // Unicode Character 'PILE OF POO' (U+1F4A9)

test(function() {
    var params;
    params = new URLSearchParams('a=b&c=d&&e&&');
    assert_equals(params.toString(), 'a=b&c=d&e=');
    params = new URLSearchParams('a = b &a=b&c=d%20');
    assert_equals(params.toString(), 'a+=+b+&a=b&c=d+');
    // The lone '=' _does_ survive the roundtrip.
    params = new URLSearchParams('a=&a=b');
    assert_equals(params.toString(), 'a=&a=b');
}, 'URLSearchParams.toString');
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.toString.call(undefined);
  }, /^TypeError: Value of `this` is not a URLSearchParams$/);
}
