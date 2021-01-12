// META: global=window,worker

promise_test(() => fetch("resources/data-urls.json").then(res => res.json()).then(runDataURLTests), "Setup.");
function runDataURLTests(tests) {
  for(let i = 0; i < tests.length; i++) {
    const input = tests[i][0],
          expectedMimeType = tests[i][1],
          expectedBody = expectedMimeType !== null ? tests[i][2] : null;
    promise_test(t => {
      if(expectedMimeType === null) {
        return promise_rejects_js(t, TypeError, fetch(input));
      } else {
        return fetch(input).then(res => {
          return res.arrayBuffer().then(body => {
            assert_array_equals(new Uint8Array(body), expectedBody);
            assert_equals(res.headers.get("content-type"), expectedMimeType); // We could assert this earlier, but this fails often
          });
        });
      }
    }, format_value(input));
  }
}
