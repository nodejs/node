// META: script=resources/test-helpers.sub.js

"use strict";

promise_test(async t => {
  const url = "resources/fetch-request-xhr-sync-error-worker.js";
  const scope = "resources/fetch-request-xhr-sync-iframe.html";

  const registration = await service_worker_unregister_and_register(t, url, scope);
  t.add_cleanup(() => registration.unregister());

  await wait_for_state(t, registration.installing, 'activated');
  const frame = await with_iframe(scope);
  t.add_cleanup(() => frame.remove());

  assert_throws_dom("NetworkError", frame.contentWindow.DOMException, () => frame.contentWindow.performSyncXHR("non-existent-stream-1.txt"));
  assert_throws_dom("NetworkError", frame.contentWindow.DOMException, () => frame.contentWindow.performSyncXHR("non-existent-stream-2.txt"));
  assert_throws_dom("NetworkError", frame.contentWindow.DOMException, () => frame.contentWindow.performSyncXHR("non-existent-stream-3.txt"));
}, "Verify synchronous XMLHttpRequest always throws a NetworkError for ReadableStream errors");
