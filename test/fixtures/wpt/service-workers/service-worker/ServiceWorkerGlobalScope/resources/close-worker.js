importScripts('../../resources/worker-testharness.js');

test(function() {
  assert_false('close' in self);
}, 'ServiceWorkerGlobalScope close operation');
