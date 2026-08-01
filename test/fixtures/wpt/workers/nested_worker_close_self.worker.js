importScripts("/resources/testharness.js");

async_test(function() {
  if (!self.Worker)
    done();
  const worker = new Worker("support/WorkerClose.js");
  worker.onmessage = this.step_func_done(function(e) {
    assert_equals(e.data, "close");
    done();
  });
  worker.postMessage("close");
}, "Nested work that closes itself");
