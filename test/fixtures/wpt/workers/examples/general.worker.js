// This file is an example of a test using *.worker.js mechanism.
// The parent document that calls fetch_tests_from_worker() is auto-generated
// but there are no generated code in the worker side.

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

test(() => {
    // This file is "general.worker.js" and this file itself is the worker
    // top-level script (which is different from the .any.js case).
    assert_equals(location.pathname, "/workers/examples/general.worker.js");
  },
  "Worker top-level script is the .worker.js file itself."
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
