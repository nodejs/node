// META: global=window,worker
// META: title=Request disturbed
// META: script=../resources/utils.js

var initValuesDict = {"method" : "POST",
                      "body" : "Request's body"
};

var noBodyConsumed = new Request("");
var bodyConsumed = new Request("", initValuesDict);

test(() => {
  assert_equals(noBodyConsumed.body, null, "body's default value is null");
  assert_false(noBodyConsumed.bodyUsed , "bodyUsed is false when request is not disturbed");
  assert_not_equals(bodyConsumed.body, null, "non-null body");
  assert_true(bodyConsumed.body instanceof ReadableStream, "non-null body type");
  assert_false(noBodyConsumed.bodyUsed, "bodyUsed is false when request is not disturbed");
}, "Request's body: initial state");

noBodyConsumed.blob();
bodyConsumed.blob();

test(function() {
  assert_false(noBodyConsumed.bodyUsed , "bodyUsed is false when request is not disturbed");
  try {
    noBodyConsumed.clone();
  } catch (e) {
    assert_unreached("Can use request not disturbed for creating or cloning request");
  }
}, "Request without body cannot be disturbed");

test(function() {
  assert_true(bodyConsumed.bodyUsed , "bodyUsed is true when request is disturbed");
  assert_throws_js(TypeError, function() { bodyConsumed.clone(); });
}, "Check cloning a disturbed request");

test(function() {
  assert_true(bodyConsumed.bodyUsed , "bodyUsed is true when request is disturbed");
  assert_throws_js(TypeError, function() { new Request(bodyConsumed); });
}, "Check creating a new request from a disturbed request");

promise_test(function() {
  assert_true(bodyConsumed.bodyUsed , "bodyUsed is true when request is disturbed");
  const originalBody = bodyConsumed.body;
  const bodyReplaced = new Request(bodyConsumed, { body: "Replaced body" });
  assert_not_equals(bodyReplaced.body, originalBody, "new request's body is new");
  assert_false(bodyReplaced.bodyUsed, "bodyUsed is false when request is not disturbed");
  return bodyReplaced.text().then(text => {
    assert_equals(text, "Replaced body");
  });
}, "Check creating a new request with a new body from a disturbed request");

promise_test(function() {
  var bodyRequest = new Request("", initValuesDict);
  const originalBody = bodyRequest.body;
  assert_false(bodyRequest.bodyUsed , "bodyUsed is false when request is not disturbed");
  var requestFromRequest = new Request(bodyRequest);
  assert_true(bodyRequest.bodyUsed , "bodyUsed is true when request is disturbed");
  assert_equals(bodyRequest.body, originalBody, "body should not change");
  assert_not_equals(originalBody, undefined, "body should not be undefined");
  assert_not_equals(originalBody, null, "body should not be null");
  assert_not_equals(requestFromRequest.body, originalBody, "new request's body is new");
  return requestFromRequest.text().then(text => {
    assert_equals(text, "Request's body");
  });
}, "Input request used for creating new request became disturbed");

promise_test(() => {
  const bodyRequest = new Request("", initValuesDict);
  const originalBody = bodyRequest.body;
  assert_false(bodyRequest.bodyUsed , "bodyUsed is false when request is not disturbed");
  const requestFromRequest = new Request(bodyRequest, { body : "init body" });
  assert_true(bodyRequest.bodyUsed , "bodyUsed is true when request is disturbed");
  assert_equals(bodyRequest.body, originalBody, "body should not change");
  assert_not_equals(originalBody, undefined, "body should not be undefined");
  assert_not_equals(originalBody, null, "body should not be null");
  assert_not_equals(requestFromRequest.body, originalBody, "new request's body is new");

  return requestFromRequest.text().then(text => {
    assert_equals(text, "init body");
  });
}, "Input request used for creating new request became disturbed even if body is not used");

promise_test(function(test) {
  assert_true(bodyConsumed.bodyUsed , "bodyUsed is true when request is disturbed");
  return promise_rejects_js(test, TypeError, bodyConsumed.blob());
}, "Check consuming a disturbed request");

test(function() {
  var req = new Request(URL, {method: 'POST', body: 'hello'});
  assert_false(req.bodyUsed,
                'Request should not be flagged as used if it has not been ' +
                'consumed.');
  assert_throws_js(TypeError,
    function() { new Request(req, {method: 'GET'}); },
    'A get request may not have body.');

  assert_false(req.bodyUsed, 'After the GET case');

  assert_throws_js(TypeError,
    function() { new Request(req, {method: 'CONNECT'}); },
    'Request() with a forbidden method must throw.');

  assert_false(req.bodyUsed, 'After the forbidden method case');

  var req2 = new Request(req);
  assert_true(req.bodyUsed,
              'Request should be flagged as used if it has been consumed.');
}, 'Request construction failure should not set "bodyUsed"');
