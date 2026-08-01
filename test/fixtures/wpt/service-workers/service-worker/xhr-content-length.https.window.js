// META: script=resources/test-helpers.sub.js

let frame;

promise_test(async (t) => {
  const scope = "resources/empty.html";
  const script = "resources/xhr-content-length-worker.js";
  const registration = await service_worker_unregister_and_register(t, script, scope);
  await wait_for_state(t, registration.installing, "activated");
  frame = await with_iframe(scope);
}, "Setup");

promise_test(async t => {
  const xhr = new frame.contentWindow.XMLHttpRequest();
  xhr.open("GET", "test?type=no-content-length");
  xhr.send();
  const event = await new Promise(resolve => xhr.onload = resolve);
  assert_equals(xhr.getResponseHeader("content-length"), null);
  assert_false(event.lengthComputable);
  assert_equals(event.total, 0);
  assert_equals(event.loaded, xhr.responseText.length);
}, `Synthetic response without Content-Length header`);

promise_test(async t => {
  const xhr = new frame.contentWindow.XMLHttpRequest();
  xhr.open("GET", "test?type=larger-content-length");
  xhr.send();
  const event = await new Promise(resolve => xhr.onload = resolve);
  assert_equals(xhr.getResponseHeader("content-length"), "10000");
  assert_true(event.lengthComputable);
  assert_equals(event.total, 10000);
  assert_equals(event.loaded, xhr.responseText.length);
}, `Synthetic response with Content-Length header with value larger than response body length`);

promise_test(async t => {
  const xhr = new frame.contentWindow.XMLHttpRequest();
  xhr.open("GET", "test?type=double-content-length");
  xhr.send();
  const event = await new Promise(resolve => xhr.onload = resolve);
  assert_equals(xhr.getResponseHeader("content-length"), "10000, 10000");
  assert_true(event.lengthComputable);
  assert_equals(event.total, 10000);
  assert_equals(event.loaded, xhr.responseText.length);
}, `Synthetic response with two Content-Length headers value larger than response body length`);

promise_test(async t => {
  const xhr = new frame.contentWindow.XMLHttpRequest();
  xhr.open("GET", "test?type=bogus-content-length");
  xhr.send();
  const event = await new Promise(resolve => xhr.onload = resolve);
  assert_equals(xhr.getResponseHeader("content-length"), "test");
  assert_false(event.lengthComputable);
  assert_equals(event.total, 0);
  assert_equals(event.loaded, xhr.responseText.length);
}, `Synthetic response with bogus Content-Length header`);
