// META: title=Cache.delete
// META: global=window,worker
// META: script=./resources/test-helpers.js
// META: timeout=long

var test_url = 'https://example.com/foo';

// Construct a generic Request object. The URL is |test_url|. All other fields
// are defaults.
function new_test_request() {
  return new Request(test_url);
}

// Construct a generic Response object.
function new_test_response() {
  return new Response('Hello world!', { status: 200 });
}

cache_test(function(cache, test) {
    return promise_rejects_js(
      test,
      TypeError,
      cache.delete(),
      'Cache.delete should reject with a TypeError when called with no ' +
      'arguments.');
  }, 'Cache.delete with no arguments');

cache_test(function(cache) {
    return cache.put(new_test_request(), new_test_response())
      .then(function() {
          return cache.delete(test_url);
        })
      .then(function(result) {
          assert_true(result,
                      'Cache.delete should resolve with "true" if an entry ' +
                      'was successfully deleted.');
          return cache.match(test_url);
        })
      .then(function(result) {
          assert_equals(result, undefined,
            'Cache.delete should remove matching entries from cache.');
        });
  }, 'Cache.delete called with a string URL');

cache_test(function(cache) {
    var request = new Request(test_url);
    return cache.put(request, new_test_response())
      .then(function() {
          return cache.delete(request);
        })
      .then(function(result) {
          assert_true(result,
                      'Cache.delete should resolve with "true" if an entry ' +
                      'was successfully deleted.');
        });
  }, 'Cache.delete called with a Request object');

cache_test(function(cache) {
    var request = new Request(test_url);
    var response = new_test_response();
    return cache.put(request, response)
      .then(function() {
          return cache.delete(new Request(test_url, {method: 'HEAD'}));
        })
      .then(function(result) {
          assert_false(result,
                       'Cache.delete should not match a non-GET request ' +
                       'unless ignoreMethod option is set.');
          return cache.match(test_url);
        })
      .then(function(result) {
          assert_response_equals(result, response,
            'Cache.delete should leave non-matching response in the cache.');
          return cache.delete(new Request(test_url, {method: 'HEAD'}),
                              {ignoreMethod: true});
        })
      .then(function(result) {
          assert_true(result,
                      'Cache.delete should match a non-GET request ' +
                      ' if ignoreMethod is true.');
        });
  }, 'Cache.delete called with a HEAD request');

cache_test(function(cache) {
    var vary_request = new Request('http://example.com/c',
                                   {headers: {'Cookies': 'is-for-cookie'}});
    var vary_response = new Response('', {headers: {'Vary': 'Cookies'}});
    var mismatched_vary_request = new Request('http://example.com/c');

    return cache.put(vary_request.clone(), vary_response.clone())
      .then(function() {
          return cache.delete(mismatched_vary_request.clone());
        })
      .then(function(result) {
          assert_false(result,
                       'Cache.delete should not delete if vary does not ' +
                       'match unless ignoreVary is true');
          return cache.delete(mismatched_vary_request.clone(),
                              {ignoreVary: true});
        })
      .then(function(result) {
          assert_true(result,
                      'Cache.delete should ignore vary if ignoreVary is true');
        });
  }, 'Cache.delete supports ignoreVary');

cache_test(function(cache) {
    return cache.delete(test_url)
      .then(function(result) {
          assert_false(result,
                       'Cache.delete should resolve with "false" if there ' +
                       'are no matching entries.');
        });
  }, 'Cache.delete with a non-existent entry');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.matchAll(entries.a_with_query.request,
                          { ignoreSearch: true })
      .then(function(result) {
          assert_response_array_equals(
            result,
            [
              entries.a.response,
              entries.a_with_query.response
            ]);
          return cache.delete(entries.a_with_query.request,
                              { ignoreSearch: true });
        })
      .then(function(result) {
          return cache.matchAll(entries.a_with_query.request,
                                { ignoreSearch: true });
        })
      .then(function(result) {
          assert_response_array_equals(result, []);
        });
  },
  'Cache.delete with ignoreSearch option (request with search parameters)');

prepopulated_cache_test(simple_entries, function(cache, entries) {
    return cache.matchAll(entries.a_with_query.request,
                          { ignoreSearch: true })
      .then(function(result) {
          assert_response_array_equals(
            result,
            [
              entries.a.response,
              entries.a_with_query.response
            ]);
          // cache.delete()'s behavior should be the same if ignoreSearch is
          // not provided or if ignoreSearch is false.
          return cache.delete(entries.a_with_query.request,
                              { ignoreSearch: false });
        })
      .then(function(result) {
          return cache.matchAll(entries.a_with_query.request,
                                { ignoreSearch: true });
        })
      .then(function(result) {
          assert_response_array_equals(result, [ entries.a.response ]);
        });
  },
  'Cache.delete with ignoreSearch option (when it is specified as false)');

done();
