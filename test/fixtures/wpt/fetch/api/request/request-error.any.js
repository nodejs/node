// META: global=window,worker
// META: title=Request error
// META: script=request-error.js

// badRequestArgTests is from response-error.js
for (const { args, testName } of badRequestArgTests) {
  test(() => {
    assert_throws_js(
      TypeError,
      () => new Request(...args),
      "Expect TypeError exception"
    );
  }, testName);
}

test(function() {
  assert_throws_js(
      TypeError,
      () => Request("about:blank"),
      "Calling Request constructor without 'new' must throw"
    );
});

test(function() {
  var initialHeaders = new Headers([["Content-Type", "potato"]]);
  var initialRequest = new Request("", {"headers" : initialHeaders});
  var request = new Request(initialRequest);
  assert_equals(request.headers.get("Content-Type"), "potato");
}, "Request should get its content-type from the init request");

test(function() {
  var initialHeaders = new Headers([["Content-Type", "potato"]]);
  var initialRequest = new Request("", {"headers" : initialHeaders});
  var headers = new Headers([]);
  var request = new Request(initialRequest, {"headers" : headers});
  assert_false(request.headers.has("Content-Type"));
}, "Request should not get its content-type from the init request if init headers are provided");

test(function() {
  var initialHeaders = new Headers([["Content-Type-Extra", "potato"]]);
  var initialRequest = new Request("", {"headers" : initialHeaders, "body" : "this is my plate", "method" : "POST"});
  var request = new Request(initialRequest);
  assert_equals(request.headers.get("Content-Type"), "text/plain;charset=UTF-8");
}, "Request should get its content-type from the body if none is provided");

test(function() {
  var initialHeaders = new Headers([["Content-Type", "potato"]]);
  var initialRequest = new Request("", {"headers" : initialHeaders, "body" : "this is my plate", "method" : "POST"});
  var request = new Request(initialRequest);
  assert_equals(request.headers.get("Content-Type"), "potato");
}, "Request should get its content-type from init headers if one is provided");

test(function() {
  var options = {"cache": "only-if-cached", "mode": "same-origin"};
  new Request("test", options);
}, "Request with cache mode: only-if-cached and fetch mode: same-origin");
