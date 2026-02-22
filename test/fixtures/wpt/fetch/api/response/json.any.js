// See also /xhr/json.any.js

promise_test(async t => {
  const response = await fetch(`data:,\uFEFF{ "b": 1, "a": 2, "b": 3 }`);
  const json = await response.json();
  assert_array_equals(Object.keys(json), ["b", "a"]);
  assert_equals(json.a, 2);
  assert_equals(json.b, 3);
}, "Ensure the correct JSON parser is used");

promise_test(async t => {
  const response = await fetch("/xhr/resources/utf16-bom.json");
  return promise_rejects_js(t, SyntaxError, response.json());
}, "Ensure UTF-16 results in an error");
