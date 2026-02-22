// META: global=window,worker
// META: script=../resources/utils.js
// META: script=/common/utils.js
// META: script=/common/get-host-info.sub.js

const redirectUrl = RESOURCES_DIR + "redirect.h2.py";
const redirectLocation = "top.txt";

async function fetchStreamRedirect(statusCode) {
  const url = RESOURCES_DIR + "redirect.h2.py" +
   `?redirect_status=${statusCode}&location=${redirectLocation}`;
  const requestInit = {method: "POST"};
  requestInit["body"] = new ReadableStream({start: controller => {
    const encoder = new TextEncoder();
    controller.enqueue(encoder.encode("Test"));
    controller.close();
  }});
  requestInit.duplex = "half";
  return fetch(url, requestInit);
}

promise_test(async () => {
  const resp = await fetchStreamRedirect(303);
  assert_equals(resp.status, 200);
  assert_true(new URL(resp.url).pathname.endsWith(redirectLocation),
   "Response's url should be the redirected one");
}, "Fetch upload streaming should be accepted on 303");

for (const statusCode of [301, 302, 307, 308]) {
  promise_test(t => {
    return promise_rejects_js(t, TypeError, fetchStreamRedirect(statusCode));
  }, `Fetch upload streaming should fail on ${statusCode}`);
}
