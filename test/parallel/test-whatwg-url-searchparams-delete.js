'use strict';

require('../common');
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
