// META: global=window,worker
// META: script=../resources/utils.js

const nullBodyStatus = [204, 205, 304];
const methods = ["GET", "POST", "OPTIONS"];

for (const status of nullBodyStatus) {
  for (const method of methods) {
    promise_test(
      async () => {
        const url =
          `${RESOURCES_DIR}status.py?code=${status}&content=hello-world`;
        const resp = await fetch(url, { method });
        assert_equals(resp.status, status);
        assert_equals(resp.body, null, "the body should be null");
        const text = await resp.text();
        assert_equals(text, "", "null bodies result in empty text");
      },
      `Response.body is null for responses with status=${status} (method=${method})`,
    );
  }
}

promise_test(async () => {
  const url = `${RESOURCES_DIR}status.py?code=200&content=hello-world`;
  const resp = await fetch(url, { method: "HEAD" });
  assert_equals(resp.status, 200);
  assert_equals(resp.body, null, "the body should be null");
  const text = await resp.text();
  assert_equals(text, "", "null bodies result in empty text");
}, `Response.body is null for responses with method=HEAD`);

promise_test(async (t) => {
  const integrity = "sha384-UT6f7WCFp32YJnp1is4l/ZYnOeQKpE8xjmdkLOwZ3nIP+tmT2aMRFQGJomjVf5cE";
  const url = `${RESOURCES_DIR}status.py?code=204&content=hello-world`;
  const promise = fetch(url, { method: "GET", integrity });
  promise_rejects_js(t, TypeError, promise);
}, "Null body status with subresource integrity should abort");
