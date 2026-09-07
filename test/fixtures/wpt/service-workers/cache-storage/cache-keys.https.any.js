// META: title=Cache.keys
// META: global=window,worker
// META: script=./resources/test-helpers.js
// META: timeout=long

cache_test(cache => {
    return cache.keys()
      .then(requests => {
          assert_equals(
            requests.length, 0,
            'Cache.keys should resolve to an empty array for an empty cache');
        });
  }, 'Cache.keys() called on an empty cache');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys('not-present-in-the-cache')
      .then(function(result) {
          assert_request_array_equals(
            result, [],
            'Cache.keys should resolve with an empty array on failure.');
        });
  }, 'Cache.keys with no matching entries');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.a.request.url)
      .then(function(result) {
          assert_request_array_equals(result, [entries.a.request],
                                      'Cache.keys should match by URL.');
        });
  }, 'Cache.keys with URL');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.a.request)
      .then(function(result) {
          assert_request_array_equals(
            result, [entries.a.request],
            'Cache.keys should match by Request.');
        });
  }, 'Cache.keys with Request');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(new Request(entries.a.request.url))
      .then(function(result) {
          assert_request_array_equals(
            result, [entries.a.request],
            'Cache.keys should match by Request.');
        });
  }, 'Cache.keys with new Request');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.a.request, {ignoreSearch: true})
      .then(function(result) {
          assert_request_array_equals(
            result,
            [
              entries.a.request,
              entries.a_with_query.request
            ],
            'Cache.keys with ignoreSearch should ignore the ' +
            'search parameters of cached request.');
        });
  },
  'Cache.keys with ignoreSearch option (request with no search ' +
  'parameters)');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.a_with_query.request, {ignoreSearch: true})
      .then(function(result) {
          assert_request_array_equals(
            result,
            [
              entries.a.request,
              entries.a_with_query.request
            ],
            'Cache.keys with ignoreSearch should ignore the ' +
            'search parameters of request.');
        });
  },
  'Cache.keys with ignoreSearch option (request with search parameters)');

cache_test(function(cache) {
    var request = new Request('http://example.com/');
    var head_request = new Request('http://example.com/', {method: 'HEAD'});
    var response = new Response('foo');
    return cache.put(request.clone(), response.clone())
      .then(function() {
          return cache.keys(head_request.clone());
        })
      .then(function(result) {
          assert_request_array_equals(
            result, [],
            'Cache.keys should resolve with an empty array with a ' +
            'mismatched method.');
          return cache.keys(head_request.clone(),
                            {ignoreMethod: true});
        })
      .then(function(result) {
          assert_request_array_equals(
            result,
            [
              request,
            ],
            'Cache.keys with ignoreMethod should ignore the ' +
            'method of request.');
        });
  }, 'Cache.keys supports ignoreMethod');

cache_test(function(cache) {
    var vary_request = new Request('http://example.com/c',
                                   {headers: {'Cookies': 'is-for-cookie'}});
    var vary_response = new Response('', {headers: {'Vary': 'Cookies'}});
    var mismatched_vary_request = new Request('http://example.com/c');

    return cache.put(vary_request.clone(), vary_response.clone())
      .then(function() {
          return cache.keys(mismatched_vary_request.clone());
        })
      .then(function(result) {
          assert_request_array_equals(
            result, [],
            'Cache.keys should resolve with an empty array with a ' +
            'mismatched vary.');
          return cache.keys(mismatched_vary_request.clone(),
                              {ignoreVary: true});
        })
      .then(function(result) {
          assert_request_array_equals(
            result,
            [
              vary_request,
            ],
            'Cache.keys with ignoreVary should ignore the ' +
            'vary of request.');
        });
  }, 'Cache.keys supports ignoreVary');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(entries.cat.request.url + '#mouse')
      .then(function(result) {
          assert_request_array_equals(
            result,
            [
              entries.cat.request,
            ],
            'Cache.keys should ignore URL fragment.');
        });
  }, 'Cache.keys with URL containing fragment');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys('http')
      .then(function(result) {
          assert_request_array_equals(
            result, [],
            'Cache.keys should treat query as a URL and not ' +
            'just a string fragment.');
        });
  }, 'Cache.keys with string fragment "http" as query');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys()
      .then(function(result) {
          assert_request_array_equals(
            result,
            simple_entries.map(entry => entry.request),
            'Cache.keys without parameters should match all entries.');
        });
  }, 'Cache.keys without parameters');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(undefined)
      .then(function(result) {
          assert_request_array_equals(
            result,
            simple_entries.map(entry => entry.request),
            'Cache.keys with undefined request should match all entries.');
        });
  }, 'Cache.keys with explicitly undefined request');

cache_test(cache => {
    return cache.keys(undefined, {})
      .then(requests => {
          assert_equals(
            requests.length, 0,
            'Cache.keys should resolve to an empty array for an empty cache');
        });
  }, 'Cache.keys with explicitly undefined request and empty options');

prepopulated_cache_test(vary_entries, function(cache, entries) {
    return cache.keys()
      .then(function(result) {
          assert_request_array_equals(
            result,
            [
              entries.vary_cookie_is_cookie.request,
              entries.vary_cookie_is_good.request,
              entries.vary_cookie_absent.request,
            ],
            'Cache.keys without parameters should match all entries.');
        });
  }, 'Cache.keys without parameters and VARY entries');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.keys(new Request(entries.cat.request.url, {method: 'HEAD'}))
      .then(function(result) {
          assert_request_array_equals(
            result, [],
            'Cache.keys should not match HEAD request unless ignoreMethod ' +
            'option is set.');
        });
  }, 'Cache.keys with a HEAD Request');

done();
