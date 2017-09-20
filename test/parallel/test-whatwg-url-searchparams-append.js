'use strict';

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;
const { test, assert_equals, assert_true } = require('../common/wpt');

/* The following tests are copied from WPT. Modifications to them should be
   upstreamed first. Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/urlsearchparams-append.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
/* eslint-disable */
test(function() {
    var params = new URLSearchParams();
    params.append('a', 'b');
    assert_equals(params + '', 'a=b');
    params.append('a', 'b');
    assert_equals(params + '', 'a=b&a=b');
    params.append('a', 'c');
    assert_equals(params + '', 'a=b&a=b&a=c');
}, 'Append same name');
test(function() {
    var params = new URLSearchParams();
    params.append('', '');
    assert_equals(params + '', '=');
    params.append('', '');
    assert_equals(params + '', '=&=');
}, 'Append empty strings');
test(function() {
    var params = new URLSearchParams();
    params.append(null, null);
    assert_equals(params + '', 'null=null');
    params.append(null, null);
    assert_equals(params + '', 'null=null&null=null');
}, 'Append null');
test(function() {
    var params = new URLSearchParams();
    params.append('first', 1);
    params.append('second', 2);
    params.append('third', '');
    params.append('first', 10);
    assert_true(params.has('first'), 'Search params object has name "first"');
    assert_equals(params.get('first'), '1', 'Search params object has name "first" with value "1"');
    assert_equals(params.get('second'), '2', 'Search params object has name "second" with value "2"');
    assert_equals(params.get('third'), '', 'Search params object has name "third" with value ""');
    params.append('first', 10);
    assert_equals(params.get('first'), '1', 'Search params object has name "first" with value "1"');
}, 'Append multiple');
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  assert.throws(() => {
    params.append.call(undefined);
  }, common.expectsError({
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type URLSearchParams'
  }));
  assert.throws(() => {
    params.append('a');
  }, common.expectsError({
    code: 'ERR_MISSING_ARGS',
    type: TypeError,
    message: 'The "name" and "value" arguments must be specified'
  }));

  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  assert.throws(() => params.set(obj, 'b'), /^Error: toString$/);
  assert.throws(() => params.set('a', obj), /^Error: toString$/);
  assert.throws(() => params.set(sym, 'b'),
                /^TypeError: Cannot convert a Symbol value to a string$/);
  assert.throws(() => params.set('a', sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
}
