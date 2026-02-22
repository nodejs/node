// META: global=window,worker
// META: title=Response: json static method

const APPLICATION_JSON = "application/json";
const FOO_BAR = "foo/bar";

const INIT_TESTS = [
  [undefined, 200, "", APPLICATION_JSON, {}],
  [{ status: 400 }, 400, "", APPLICATION_JSON, {}],
  [{ statusText: "foo" }, 200, "foo", APPLICATION_JSON, {}],
  [{ headers: {} }, 200, "", APPLICATION_JSON, {}],
  [{ headers: { "content-type": FOO_BAR } }, 200, "", FOO_BAR, {}],
  [{ headers: { "x-foo": "bar" } }, 200, "", APPLICATION_JSON, { "x-foo": "bar" }],
];

for (const [init, expectedStatus, expectedStatusText, expectedContentType, expectedHeaders] of INIT_TESTS) {
  promise_test(async function () {
    const response = Response.json("hello world", init);
    assert_equals(response.type, "default", "Response's type is default");
    assert_equals(response.status, expectedStatus, "Response's status is " + expectedStatus);
    assert_equals(response.statusText, expectedStatusText, "Response's statusText is " + JSON.stringify(expectedStatusText));
    assert_equals(response.headers.get("content-type"), expectedContentType, "Response's content-type is " + expectedContentType);
    for (const key in expectedHeaders) {
      assert_equals(response.headers.get(key), expectedHeaders[key], "Response's header " + key + " is " + JSON.stringify(expectedHeaders[key]));
    }

    const data = await response.json();
    assert_equals(data, "hello world", "Response's body is 'hello world'");
  }, `Check response returned by static json() with init ${JSON.stringify(init)}`);
}

const nullBodyStatus = [204, 205, 304];
for (const status of nullBodyStatus) {
  test(function () {
    assert_throws_js(
      TypeError,
      function () {
        Response.json("hello world", { status: status });
      },
    );
  }, `Throws TypeError when calling static json() with a status of ${status}`);
}

promise_test(async function () {
  const response = Response.json({ foo: "bar" });
  const data = await response.json();
  assert_equals(typeof data, "object", "Response's json body is an object");
  assert_equals(data.foo, "bar", "Response's json body is { foo: 'bar' }");
}, "Check static json() encodes JSON objects correctly");

test(function () {
  assert_throws_js(
    TypeError,
    function () {
      Response.json(Symbol("foo"));
    },
  );
}, "Check static json() throws when data is not encodable");

test(function () {
  const a = { b: 1 };
  a.a = a;
  assert_throws_js(
    TypeError,
    function () {
      Response.json(a);
    },
  );
}, "Check static json() throws when data is circular");

promise_test(async function () {
  class CustomError extends Error {
    name = "CustomError";
  }
  assert_throws_js(
    CustomError,
    function () {
      Response.json({ get foo() { throw new CustomError("bar") }});
    }
  )
}, "Check static json() propagates JSON serializer errors");

const encodingChecks = [
  ["𝌆", [34, 240, 157, 140, 134, 34]],
  ["\uDF06\uD834", [34, 92, 117, 100, 102, 48, 54, 92, 117, 100, 56, 51, 52, 34]],
  ["\uDEAD", [34, 92, 117, 100, 101, 97, 100, 34]],
];

for (const [input, expected] of encodingChecks) {
  promise_test(async function () {
    const response = Response.json(input);
    const buffer = await response.arrayBuffer();
    const data = new Uint8Array(buffer);
    assert_array_equals(data, expected);
  }, `Check response returned by static json() with input ${input}`);
}
