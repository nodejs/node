// META: global=window,worker
// META: title=Request Headers

var validRequestHeaders = [
  ["Content-Type", "OK"],
  ["Potato", "OK"],
  ["proxy", "OK"],
  ["proxya", "OK"],
  ["sec", "OK"],
  ["secb", "OK"],
  ["Set-Cookie2", "OK"],
  ["User-Agent", "OK"],
];
var invalidRequestHeaders = [
  ["Accept-Charset", "KO"],
  ["accept-charset", "KO"],
  ["ACCEPT-ENCODING", "KO"],
  ["Accept-Encoding", "KO"],
  ["Access-Control-Request-Headers", "KO"],
  ["Access-Control-Request-Method", "KO"],
  ["Connection", "KO"],
  ["Content-Length", "KO"],
  ["Cookie", "KO"],
  ["Cookie2", "KO"],
  ["Date", "KO"],
  ["DNT", "KO"],
  ["Expect", "KO"],
  ["Host", "KO"],
  ["Keep-Alive", "KO"],
  ["Origin", "KO"],
  ["Referer", "KO"],
  ["Set-Cookie", "KO"],
  ["TE", "KO"],
  ["Trailer", "KO"],
  ["Transfer-Encoding", "KO"],
  ["Upgrade", "KO"],
  ["Via", "KO"],
  ["Proxy-", "KO"],
  ["proxy-a", "KO"],
  ["Sec-", "KO"],
  ["sec-b", "KO"],
];

var validRequestNoCorsHeaders = [
  ["Accept", "OK"],
  ["Accept-Language", "OK"],
  ["content-language", "OK"],
  ["content-type", "application/x-www-form-urlencoded"],
  ["content-type", "application/x-www-form-urlencoded;charset=UTF-8"],
  ["content-type", "multipart/form-data"],
  ["content-type", "multipart/form-data;charset=UTF-8"],
  ["content-TYPE", "text/plain"],
  ["CONTENT-type", "text/plain;charset=UTF-8"],
];
var invalidRequestNoCorsHeaders = [
  ["Content-Type", "KO"],
  ["Potato", "KO"],
  ["proxy", "KO"],
  ["proxya", "KO"],
  ["sec", "KO"],
  ["secb", "KO"],
  ["Empty-Value", ""],
];

validRequestHeaders.forEach(function(header) {
  test(function() {
    var request = new Request("");
    request.headers.set(header[0], header[1]);
    assert_equals(request.headers.get(header[0]), header[1]);
  }, "Adding valid request header \"" + header[0] + ": " + header[1] + "\"");
});
invalidRequestHeaders.forEach(function(header) {
  test(function() {
    var request = new Request("");
    request.headers.set(header[0], header[1]);
    assert_equals(request.headers.get(header[0]), null);
  }, "Adding invalid request header \"" + header[0] + ": " + header[1] + "\"");
});

validRequestNoCorsHeaders.forEach(function(header) {
  test(function() {
    var requestNoCors = new Request("", {"mode": "no-cors"});
    requestNoCors.headers.set(header[0], header[1]);
    assert_equals(requestNoCors.headers.get(header[0]), header[1]);
  }, "Adding valid no-cors request header \"" + header[0] + ": " + header[1] + "\"");
});
invalidRequestNoCorsHeaders.forEach(function(header) {
  test(function() {
    var requestNoCors = new Request("", {"mode": "no-cors"});
    requestNoCors.headers.set(header[0], header[1]);
    assert_equals(requestNoCors.headers.get(header[0]), null);
  }, "Adding invalid no-cors request header \"" + header[0] + ": " + header[1] + "\"");
});

test(function() {
  var headers = new Headers([["Cookie2", "potato"]]);
  var request = new Request("", {"headers": headers});
  assert_equals(request.headers.get("Cookie2"), null);
}, "Check that request constructor is filtering headers provided as init parameter");

test(function() {
  var headers = new Headers([["Content-Type", "potato"]]);
  var request = new Request("", {"headers": headers, "mode": "no-cors"});
  assert_equals(request.headers.get("Content-Type"), null);
}, "Check that no-cors request constructor is filtering headers provided as init parameter");

test(function() {
  var headers = new Headers([["Content-Type", "potato"]]);
  var initialRequest = new Request("", {"headers": headers});
  var request = new Request(initialRequest, {"mode": "no-cors"});
  assert_equals(request.headers.get("Content-Type"), null);
}, "Check that no-cors request constructor is filtering headers provided as part of request parameter");

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
  var array = [["hello", "worldAHH"]];
  var object = {"hello": 'worldOOH'};
  var headers = new Headers(array);

  assert_equals(headers.get("hello"), "worldAHH");

  var request1 = new Request("", {"headers": headers});
  var request2 = new Request("", {"headers": array});
  var request3 = new Request("", {"headers": object});

  assert_equals(request1.headers.get("hello"), "worldAHH");
  assert_equals(request2.headers.get("hello"), "worldAHH");
  assert_equals(request3.headers.get("hello"), "worldOOH");
}, "Testing request header creations with various objects");

promise_test(function(test) {
  var request = new Request("", {"headers" : [["Content-Type", ""]], "body" : "this is my plate", "method" : "POST"});
  return request.blob().then(function(blob) {
    assert_equals(blob.type, "", "Blob type should be the empty string");
  });
}, "Testing empty Request Content-Type header");

test(function() {
  const request1 = new Request("");
  assert_equals(request1.headers, request1.headers);

  const request2 = new Request("", {"headers": {"X-Foo": "bar"}});
  assert_equals(request2.headers, request2.headers);
  const headers = request2.headers;
  request2.headers.set("X-Foo", "quux");
  assert_equals(headers, request2.headers);
  headers.set("X-Other-Header", "baz");
  assert_equals(headers, request2.headers);
}, "Test that Request.headers has the [SameObject] extended attribute");
