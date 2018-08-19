importScripts("/resources/testharness.js");

// The following sub-test ensures that the worker is not interpreted as a
// single-page test.  The subsequent uncaught exception should therefore be
// interpreted as a harness error rather than a single-page test failure.
test(function() {}, "worker test that completes successfully before exception");

throw new Error("This failure is expected.");
