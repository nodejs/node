// META: global=window,worker

promise_test(() => fetch("resources/base64.json").then(res => res.json()).then(runBase64Tests), "Setup.");
function runBase64Tests(tests) {
  for(let i = 0; i < tests.length; i++) {
    const input = tests[i][0],
          output = tests[i][1],
          dataURL = "data:;base64," + input;
    promise_test(t => {
      if(output === null) {
        return promise_rejects_js(t, TypeError, fetch(dataURL));
      }
      return fetch(dataURL).then(res => res.arrayBuffer()).then(body => {
        assert_array_equals(new Uint8Array(body), output);
      });
    }, "data: URL base64 handling: " + format_value(input));
  }
}
