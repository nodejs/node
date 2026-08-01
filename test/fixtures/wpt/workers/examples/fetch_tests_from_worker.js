// This file is an example of a hand-written test using
// fetch_tests_from_worker().
// Unlike *.any.js or *.worker.js tests, fetch_tests_from_worker.html/js files
// are manually written and no generated glue code are involved.

// fetch_tests_from_worker() requires testharness.js both on the parent
// document and on the worker.
importScripts("/resources/testharness.js");

// ============================================================================

// Test body.
test(() => {
    assert_equals(1, 1, "1 == 1");
  },
  "Test that should pass"
);

// ============================================================================

// `done()` is always needed at the bottom for dedicated workers and shared
// workers, even if you write `async_test()` or `promise_test()`.
// `async_test()` and `promise_test()` called before this `done()`
// will continue and assertions/failures after this `done()` are not ignored.
// See
// https://web-platform-tests.org/writing-tests/testharness-api.html#determining-when-all-tests-are-complete
// for details.
done();
