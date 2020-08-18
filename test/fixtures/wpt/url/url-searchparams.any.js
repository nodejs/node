function bURL(url, base) {
  return new URL(url, base || "about:blank")
}

function runURLSearchParamTests() {
  test(function() {
    var url = bURL('http://example.org/?a=b')
    assert_true("searchParams" in url)
    var searchParams = url.searchParams
    assert_true(url.searchParams === searchParams, 'Object identity should hold.')
  }, 'URL.searchParams getter')

  test(function() {
    var url = bURL('http://example.org/?a=b')
    assert_true("searchParams" in url)
    var searchParams = url.searchParams
    assert_equals(searchParams.toString(), 'a=b')

    searchParams.set('a', 'b')
    assert_equals(url.searchParams.toString(), 'a=b')
    assert_equals(url.search, '?a=b')
    url.search = ''
    assert_equals(url.searchParams.toString(), '')
    assert_equals(url.search, '')
    assert_equals(searchParams.toString(), '')
  }, 'URL.searchParams updating, clearing')

  test(function() {
    'use strict'
    var urlString = 'http://example.org'
    var url = bURL(urlString)
    assert_throws_js(TypeError, function() { url.searchParams = new URLSearchParams(urlString) })
  }, 'URL.searchParams setter, invalid values')

  test(function() {
    var url = bURL('http://example.org/file?a=b&c=d')
    assert_true("searchParams" in url)
    var searchParams = url.searchParams
    assert_equals(url.search, '?a=b&c=d')
    assert_equals(searchParams.toString(), 'a=b&c=d')

    // Test that setting 'search' propagates to the URL object's query object.
    url.search = 'e=f&g=h'
    assert_equals(url.search, '?e=f&g=h')
    assert_equals(searchParams.toString(), 'e=f&g=h')

    // ..and same but with a leading '?'.
    url.search = '?e=f&g=h'
    assert_equals(url.search, '?e=f&g=h')
    assert_equals(searchParams.toString(), 'e=f&g=h')

    // And in the other direction, altering searchParams propagates
    // back to 'search'.
    searchParams.append('i', ' j ')
    assert_equals(url.search, '?e=f&g=h&i=+j+')
    assert_equals(url.searchParams.toString(), 'e=f&g=h&i=+j+')
    assert_equals(searchParams.get('i'), ' j ')

    searchParams.set('e', 'updated')
    assert_equals(url.search, '?e=updated&g=h&i=+j+')
    assert_equals(searchParams.get('e'), 'updated')

    var url2 = bURL('http://example.org/file??a=b&c=d')
    assert_equals(url2.search, '??a=b&c=d')
    assert_equals(url2.searchParams.toString(), '%3Fa=b&c=d')

    url2.href = 'http://example.org/file??a=b'
    assert_equals(url2.search, '??a=b')
    assert_equals(url2.searchParams.toString(), '%3Fa=b')
  }, 'URL.searchParams and URL.search setters, update propagation')
}
runURLSearchParamTests()
