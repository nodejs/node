/*
 * worker-test-harness should be considered a temporary polyfill around
 * testharness.js for supporting Service Worker based tests. It should not be
 * necessary once the test harness is able to drive worker based tests natively.
 * See https://github.com/w3c/testharness.js/pull/82 for status of effort to
 * update upstream testharness.js. Once the upstreaming is complete, tests that
 * reference worker-test-harness should be updated to directly import
 * testharness.js.
 */

importScripts('/resources/testharness.js');

(function() {
  var next_cache_index = 1;

  // Returns a promise that resolves to a newly created Cache object. The
  // returned Cache will be destroyed when |test| completes.
  function create_temporary_cache(test) {
    var uniquifier = String(++next_cache_index);
    var cache_name = self.location.pathname + '/' + uniquifier;

    test.add_cleanup(function() {
        return self.caches.delete(cache_name);
      });

    return self.caches.delete(cache_name)
      .then(function() {
          return self.caches.open(cache_name);
        });
  }

  self.create_temporary_cache = create_temporary_cache;
})();

// Runs |test_function| with a temporary unique Cache passed in as the only
// argument. The function is run as a part of Promise chain owned by
// promise_test(). As such, it is expected to behave in a manner identical (with
// the exception of the argument) to a function passed into promise_test().
//
// E.g.:
//    cache_test(function(cache) {
//      // Do something with |cache|, which is a Cache object.
//    }, "Some Cache test");
function cache_test(test_function, description) {
  promise_test(function(test) {
      return create_temporary_cache(test)
        .then(test_function);
    }, description);
}
