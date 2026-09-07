importScripts("/resources/testharness.js");

async_test(function() {
    var worker1 = new Worker("support/WorkerBasic.js");
    worker1.postMessage("ping");
    worker1.onmessage = this.step_func_done(function(evt) {
        assert_equals(evt.data, "Pass");
        worker1.terminate();
    });
}, "Nested worker");
done();
