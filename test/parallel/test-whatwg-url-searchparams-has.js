'use strict';

const common = require('../common');
const assert = require('assert');
const URLSearchParams = require('url').URLSearchParams;
const { test, assert_false, assert_true } = require('../common/wpt');

/* The following tests are copied from WPT. Modifications to them should be
   upstreamed first. Refs:
   https://github.com/w3c/web-platform-tests/blob/8791bed/url/urlsearchparams-has.html
   License: http://www.w3.org/Consortium/Legal/2008/04-testsuite-copyright.html
*/
/* eslint-disable */
test(function() {
    var params = new URLSearchParams('a=b&c=d');
    assert_true(params.has('a'));
    assert_true(params.has('c'));
    assert_false(params.has('e'));
    params = new URLSearchParams('a=b&c=d&a=e');
    assert_true(params.has('a'));
    params = new URLSearchParams('=b&c=d');
    assert_true(params.has(''));
    params = new URLSearchParams('null=a');
    assert_true(params.has(null));
}, 'Has basics');

test(function() {
    var params = new URLSearchParams('a=b&c=d&&');
    params.append('first', 1);
    params.append('first', 2);
    assert_true(params.has('a'), 'Search params object has name "a"');
    assert_true(params.has('c'), 'Search params object has name "c"');
    assert_true(params.has('first'), 'Search params object has name "first"');
    assert_false(params.has('d'), 'Search params object has no name "d"');
    params.delete('first');
    assert_false(params.has('first'), 'Search params object has no name "first"');
}, 'has() following delete()');
/* eslint-enable */

// Tests below are not from WPT.
{
  const params = new URLSearchParams();
  common.expectsError(() => {
    params.has.call(undefined);
  }, {
    code: 'ERR_INVALID_THIS',
    type: TypeError,
    message: 'Value of "this" must be of type URLSearchParams'
  });
  common.expectsError(() => {
    params.has();
  }, {
    code: 'ERR_MISSING_ARGS',
    type: TypeError,
    message: 'The "name" argument must be specified'
  });

  const obj = {
    toString() { throw new Error('toString'); },
    valueOf() { throw new Error('valueOf'); }
  };
  const sym = Symbol();
  assert.throws(() => params.has(obj), /^Error: toString$/);
  assert.throws(() => params.has(sym),
                /^TypeError: Cannot convert a Symbol value to a string$/);
}
