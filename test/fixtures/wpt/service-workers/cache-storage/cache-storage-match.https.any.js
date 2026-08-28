// META: title=CacheStorage.match
// META: global=window,worker
// META: script=./resources/test-helpers.js
// META: timeout=long

(function() {
  var next_index = 1;

  // Returns a transaction (request, response, and url) for a unique URL.
  function create_unique_transaction(test) {
    var uniquifier = String(next_index++);
    var url = 'http://example.com/' + uniquifier;

    return {
      request: new Request(url),
      response: new Response('hello'),
      url: url
    };
  }

  self.create_unique_transaction = create_unique_transaction;
})();

cache_test(function(cache) {
    var transaction = create_unique_transaction();

    return cache.put(transaction.request.clone(), transaction.response.clone())
      .then(function() {
          return self.caches.match(transaction.request);
        })
      .then(function(response) {
          assert_response_equals(response, transaction.response,
                                 'The response should not have changed.');
        });
}, 'CacheStorageMatch with no cache name provided');

cache_test(function(cache) {
    var transaction = create_unique_transaction();

    var test_cache_list = ['a', 'b', 'c'];
    return cache.put(transaction.request.clone(), transaction.response.clone())
      .then(function() {
          return Promise.all(test_cache_list.map(function(key) {
              return self.caches.open(key);
            }));
        })
      .then(function() {
          return self.caches.match(transaction.request);
        })
      .then(function(response) {
          assert_response_equals(response, transaction.response,
                                 'The response should not have changed.');
        });
}, 'CacheStorageMatch from one of many caches');

promise_test(function(test) {
    var transaction = create_unique_transaction();

    var test_cache_list = ['x', 'y', 'z'];
    return Promise.all(test_cache_list.map(function(key) {
        return self.caches.open(key);
      }))
      .then(function() { return self.caches.open('x'); })
      .then(function(cache) {
          return cache.put(transaction.request.clone(),
                           transaction.response.clone());
        })
      .then(function() {
          return self.caches.match(transaction.request, {cacheName: 'x'});
        })
      .then(function(response) {
          assert_response_equals(response, transaction.response,
                                 'The response should not have changed.');
        })
      .then(function() {
          return self.caches.match(transaction.request, {cacheName: 'y'});
        })
      .then(function(response) {
          assert_equals(response, undefined,
                        'Cache y should not have a response for the request.');
        });
}, 'CacheStorageMatch from one of many caches by name');

cache_test(function(cache) {
    var transaction = create_unique_transaction();
    return cache.put(transaction.url, transaction.response.clone())
      .then(function() {
          return self.caches.match(transaction.request);
        })
      .then(function(response) {
          assert_response_equals(response, transaction.response,
                                 'The response should not have changed.');
        });
}, 'CacheStorageMatch a string request');

cache_test(function(cache) {
    var transaction = create_unique_transaction();
    return cache.put(transaction.request.clone(), transaction.response.clone())
      .then(function() {
          return self.caches.match(new Request(transaction.request.url,
                                              {method: 'HEAD'}));
        })
      .then(function(response) {
          assert_equals(response, undefined,
                        'A HEAD request should not be matched');
        });
}, 'CacheStorageMatch a HEAD request');

promise_test(function(test) {
    var transaction = create_unique_transaction();
    return self.caches.match(transaction.request)
      .then(function(response) {
          assert_equals(response, undefined,
                        'The response should not be found.');
        });
}, 'CacheStorageMatch with no cached entry');

promise_test(function(test) {
    var transaction = create_unique_transaction();
    return self.caches.delete('foo')
      .then(function() {
          return self.caches.has('foo');
        })
      .then(function(has_foo) {
          assert_false(has_foo, "The cache should not exist.");
          return self.caches.match(transaction.request, {cacheName: 'foo'});
        })
      .then(function(response) {
          assert_equals(response, undefined,
                        'The match with bad cache name should resolve to ' +
                        'undefined.');
          return self.caches.has('foo');
        })
      .then(function(has_foo) {
          assert_false(has_foo, "The cache should still not exist.");
        });
}, 'CacheStorageMatch with no caches available but name provided');

cache_test(function(cache) {
    var transaction = create_unique_transaction();

    return self.caches.delete('')
      .then(function() {
          return self.caches.has('');
        })
      .then(function(has_cache) {
          assert_false(has_cache, "The cache should not exist.");
          return cache.put(transaction.request, transaction.response.clone());
        })
      .then(function() {
          return self.caches.match(transaction.request, {cacheName: ''});
        })
      .then(function(response) {
          assert_equals(response, undefined,
                        'The response should not be found.');
          return self.caches.open('');
        })
      .then(function(cache) {
          return cache.put(transaction.request, transaction.response);
        })
      .then(function() {
          return self.caches.match(transaction.request, {cacheName: ''});
        })
      .then(function(response) {
          assert_response_equals(response, transaction.response,
                                 'The response should be matched.');
          return self.caches.delete('');
        });
}, 'CacheStorageMatch with empty cache name provided');

cache_test(function(cache) {
    var request = new Request('http://example.com/?foo');
    var no_query_request = new Request('http://example.com/');
    var response = new Response('foo');
    return cache.put(request.clone(), response.clone())
      .then(function() {
          return self.caches.match(no_query_request.clone());
        })
      .then(function(result) {
          assert_equals(
            result, undefined,
            'CacheStorageMatch should resolve as undefined with a ' +
            'mismatched query.');
          return self.caches.match(no_query_request.clone(),
                                   {ignoreSearch: true});
        })
      .then(function(result) {
          assert_response_equals(
            result, response,
            'CacheStorageMatch with ignoreSearch should ignore the ' +
            'query of the request.');
        });
  }, 'CacheStorageMatch supports ignoreSearch');

cache_test(function(cache) {
    var request = new Request('http://example.com/');
    var head_request = new Request('http://example.com/', {method: 'HEAD'});
    var response = new Response('foo');
    return cache.put(request.clone(), response.clone())
      .then(function() {
          return self.caches.match(head_request.clone());
        })
      .then(function(result) {
          assert_equals(
            result, undefined,
            'CacheStorageMatch should resolve as undefined with a ' +
            'mismatched method.');
          return self.caches.match(head_request.clone(),
                                   {ignoreMethod: true});
        })
      .then(function(result) {
          assert_response_equals(
            result, response,
            'CacheStorageMatch with ignoreMethod should ignore the ' +
            'method of request.');
        });
  }, 'Cache.match supports ignoreMethod');

cache_test(function(cache) {
    var vary_request = new Request('http://example.com/c',
                                   {headers: {'Cookies': 'is-for-cookie'}});
    var vary_response = new Response('', {headers: {'Vary': 'Cookies'}});
    var mismatched_vary_request = new Request('http://example.com/c');

    return cache.put(vary_request.clone(), vary_response.clone())
      .then(function() {
          return self.caches.match(mismatched_vary_request.clone());
        })
      .then(function(result) {
          assert_equals(
            result, undefined,
            'CacheStorageMatch should resolve as undefined with a ' +
            ' mismatched vary.');
          return self.caches.match(mismatched_vary_request.clone(),
                                   {ignoreVary: true});
        })
      .then(function(result) {
          assert_response_equals(
            result, vary_response,
            'CacheStorageMatch with ignoreVary should ignore the ' +
            'vary of request.');
        });
  }, 'CacheStorageMatch supports ignoreVary');

done();
