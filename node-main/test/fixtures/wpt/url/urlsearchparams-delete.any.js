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

test(() => {
  const url = new URL('data:space    ?test');
  assert_true(url.searchParams.has('test'));
  url.searchParams.delete('test');
  assert_false(url.searchParams.has('test'));
  assert_equals(url.search, '');
  assert_equals(url.pathname, 'space   %20');
  assert_equals(url.href, 'data:space   %20');
}, 'Changing the query of a URL with an opaque path with trailing spaces');

test(() => {
  const url = new URL('data:space    ?test#test');
  url.searchParams.delete('test');
  assert_equals(url.search, '');
  assert_equals(url.pathname, 'space   %20');
  assert_equals(url.href, 'data:space   %20#test');
}, 'Changing the query of a URL with an opaque path with trailing spaces and a fragment');

test(() => {
  const params = new URLSearchParams();
  params.append('a', 'b');
  params.append('a', 'c');
  params.append('a', 'd');
  params.delete('a', 'c');
  assert_equals(params.toString(), 'a=b&a=d');
}, "Two-argument delete()");

test(() => {
  const params = new URLSearchParams();
  params.append('a', 'b');
  params.append('a', 'c');
  params.append('b', 'c');
  params.append('b', 'd');
  params.delete('b', 'c');
  params.delete('a', undefined);
  assert_equals(params.toString(), 'b=d');
}, "Two-argument delete() respects undefined as second arg");
