importScripts("/resources/testharness.js");

async_test(function() {
  const worker = new Worker('resources/worker_with_images.js');
  worker.onmessage = this.step_func_done((event) => {
    const childNumEntries = event.data;
    assert_equals(2, childNumEntries,
      "There should be two resource timing entries: 2 image XHRs");

    const parentNumEntries = performance.getEntries().length;
    assert_equals(2, parentNumEntries,
      "There should be two resource timing entries: " +
      "one is for importScripts() and the another is for a nested worker");
    worker.terminate();
  });
}, "Resource timing for nested dedicated workers");
done();
