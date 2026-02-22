// META: global=window,worker
// META: title=Response: error static method

promise_test (async () => {
  const response = await fetch("../resources/data.json");
  assert_throws_js(TypeError, () => { response.headers.append("name", "value"); });
  assert_not_equals(response.headers.get("name"), "value", "response headers should be immutable");
}, "Ensure response headers are immutable");
