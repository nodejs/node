importScripts("/resources/testharness.js");

var expected = [
  'postMessage', 'onmessage', /* DedicatedWorkerGlobalScope */
  'self', 'location', 'close', 'onerror', 'onoffline', 'ononline', /* WorkerGlobalScope */
  'addEventListener', 'removeEventListener', 'dispatchEvent', /* EventListener */
  'importScripts', 'navigator', /* WorkerUtils */
  'setTimeout', 'clearTimeout', 'setInterval', 'clearInterval', /* WindowTimers */
  'btoa', 'atob' /* WindowBase64 */
];
for (var i = 0; i < expected.length; ++i) {
  var property = expected[i];
  test(function() {
    assert_true(property in self);
  }, "existence of " + property);
}

done();
