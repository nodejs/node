// META: title=CacheStorage
// META: global=window,worker
// META: script=./resources/test-helpers.js
// META: timeout=long

promise_test(function(t) {
    var cache_name = 'cache-storage/foo';
    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(cache) {
          assert_true(cache instanceof Cache,
                      'CacheStorage.open should return a Cache.');
        });
  }, 'CacheStorage.open');

promise_test(function(t) {
    var cache_name = 'cache-storage/bar';
    var first_cache = null;
    var second_cache = null;
    return self.caches.open(cache_name)
      .then(function(cache) {
          first_cache = cache;
          return self.caches.delete(cache_name);
        })
      .then(function() {
          return first_cache.add('./resources/simple.txt');
        })
      .then(function() {
          return self.caches.keys();
        })
      .then(function(cache_names) {
          assert_equals(cache_names.indexOf(cache_name), -1);
          return self.caches.open(cache_name);
        })
      .then(function(cache) {
          second_cache = cache;
          return second_cache.keys();
        })
      .then(function(keys) {
          assert_equals(keys.length, 0);
          return first_cache.keys();
        })
      .then(function(keys) {
          assert_equals(keys.length, 1);
          // Clean up
          return self.caches.delete(cache_name);
        });
  }, 'CacheStorage.delete dooms, but does not delete immediately');

promise_test(function(t) {
    // Note that this test may collide with other tests running in the same
    // origin that also uses an empty cache name.
    var cache_name = '';
    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(cache) {
          assert_true(cache instanceof Cache,
                      'CacheStorage.open should accept an empty name.');
        });
  }, 'CacheStorage.open with an empty name');

promise_test(function(t) {
    return promise_rejects_js(
      t,
      TypeError,
      self.caches.open(),
      'CacheStorage.open should throw TypeError if called with no arguments.');
  }, 'CacheStorage.open with no arguments');

promise_test(function(t) {
    var test_cases = [
      {
        name: 'cache-storage/lowercase',
        should_not_match:
          [
            'cache-storage/Lowercase',
            ' cache-storage/lowercase',
            'cache-storage/lowercase '
          ]
      },
      {
        name: 'cache-storage/has a space',
        should_not_match:
          [
            'cache-storage/has'
          ]
      },
      {
        name: 'cache-storage/has\000_in_the_name',
        should_not_match:
          [
            'cache-storage/has',
            'cache-storage/has_in_the_name'
          ]
      }
    ];
    return Promise.all(test_cases.map(function(testcase) {
        var cache_name = testcase.name;
        return self.caches.delete(cache_name)
          .then(function() {
              return self.caches.open(cache_name);
            })
          .then(function() {
              return self.caches.has(cache_name);
            })
          .then(function(result) {
              assert_true(result,
                          'CacheStorage.has should return true for existing ' +
                          'cache.');
            })
          .then(function() {
              return Promise.all(
                testcase.should_not_match.map(function(cache_name) {
                    return self.caches.has(cache_name)
                      .then(function(result) {
                          assert_false(result,
                                       'CacheStorage.has should only perform ' +
                                       'exact matches on cache names.');
                        });
                  }));
            })
          .then(function() {
              return self.caches.delete(cache_name);
            });
      }));
  }, 'CacheStorage.has with existing cache');

promise_test(function(t) {
    return self.caches.has('cheezburger')
      .then(function(result) {
          assert_false(result,
                       'CacheStorage.has should return false for ' +
                       'nonexistent cache.');
        });
  }, 'CacheStorage.has with nonexistent cache');

promise_test(function(t) {
    var cache_name = 'cache-storage/open';
    var cache;
    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(result) {
          cache = result;
        })
      .then(function() {
          return cache.add('./resources/simple.txt');
        })
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function(result) {
          assert_true(result instanceof Cache,
                      'CacheStorage.open should return a Cache object');
          assert_not_equals(result, cache,
                            'CacheStorage.open should return a new Cache ' +
                            'object each time its called.');
          return Promise.all([cache.keys(), result.keys()]);
        })
      .then(function(results) {
          var expected_urls = results[0].map(function(r) { return r.url });
          var actual_urls = results[1].map(function(r) { return r.url });
          assert_array_equals(actual_urls, expected_urls,
                              'CacheStorage.open should return a new Cache ' +
                              'object for the same backing store.');
        });
  }, 'CacheStorage.open with existing cache');

promise_test(function(t) {
    var cache_name = 'cache-storage/delete';

    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        })
      .then(function() { return self.caches.delete(cache_name); })
      .then(function(result) {
          assert_true(result,
                      'CacheStorage.delete should return true after ' +
                      'deleting an existing cache.');
        })

      .then(function() { return self.caches.has(cache_name); })
      .then(function(cache_exists) {
          assert_false(cache_exists,
                       'CacheStorage.has should return false after ' +
                       'fulfillment of CacheStorage.delete promise.');
        });
  }, 'CacheStorage.delete with existing cache');

promise_test(function(t) {
    return self.caches.delete('cheezburger')
      .then(function(result) {
          assert_false(result,
                       'CacheStorage.delete should return false for a ' +
                       'nonexistent cache.');
        });
  }, 'CacheStorage.delete with nonexistent cache');

promise_test(function(t) {
    var unpaired_name = 'unpaired\uD800';
    var converted_name = 'unpaired\uFFFD';

    // The test assumes that a cache with converted_name does not
    // exist, but if the implementation fails the test then such
    // a cache will be created. Start off in a fresh state by
    // deleting all caches.
    return delete_all_caches()
      .then(function() {
          return self.caches.has(converted_name);
      })
      .then(function(cache_exists) {
          assert_false(cache_exists,
                       'Test setup failure: cache should not exist');
      })
      .then(function() { return self.caches.open(unpaired_name); })
      .then(function() { return self.caches.keys(); })
      .then(function(keys) {
          assert_true(keys.indexOf(unpaired_name) !== -1,
                      'keys should include cache with bad name');
      })
      .then(function() { return self.caches.has(unpaired_name); })
      .then(function(cache_exists) {
          assert_true(cache_exists,
                      'CacheStorage names should be not be converted.');
        })
      .then(function() { return self.caches.has(converted_name); })
      .then(function(cache_exists) {
          assert_false(cache_exists,
                       'CacheStorage names should be not be converted.');
        });
  }, 'CacheStorage names are DOMStrings not USVStrings');

done();
