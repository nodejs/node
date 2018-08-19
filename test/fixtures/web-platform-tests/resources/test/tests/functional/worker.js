importScripts("/resources/testharness.js");

test(
    function(test) {
        assert_true(true, "True is true");
    },
    "Worker test that completes successfully");

test(
    function(test) {
        assert_true(false, "Failing test");
    },
    "Worker test that fails ('FAIL')");

async_test(
    function(test) {
        assert_true(true, "True is true");
    },
    "Worker test that times out ('TIMEOUT')");

async_test("Worker test that doesn't run ('NOT RUN')");

async_test(
    function(test) {
        self.setTimeout(
            function() {
                test.done();
            },
            0);
    },
    "Worker async_test that completes successfully");

// An explicit done() is required for dedicated and shared web workers.
done();
