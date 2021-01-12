// META: title=Request ETag
// META: global=window,worker
// META: script=/common/utils.js

promise_test(function() {
  var cacheBuster = token(); // ensures first request is uncached
  var url = "../resources/cache.py?v=" + cacheBuster;
  var etag;

  // make the first request
  return fetch(url).then(function(response) {
    // ensure we're getting the regular, uncached response
    assert_equals(response.status, 200);
    assert_equals(response.headers.get("X-HTTP-STATUS"), null)

    return response.text(); // consuming the body, just to be safe
  }).then(function(body) {
    // make a second request
    return fetch(url);
  }).then(function(response) {
    // while the server responds with 304 if our browser sent the correct
    // If-None-Match request header, at the JavaScript level this surfaces
    // as 200
    assert_equals(response.status, 200);
    assert_equals(response.headers.get("X-HTTP-STATUS"), "304")

    etag = response.headers.get("ETag")

    return response.text(); // consuming the body, just to be safe
  }).then(function(body) {
    // make a third request, explicitly setting If-None-Match request header
    var headers = { "If-None-Match": etag }
    return fetch(url, { headers: headers })
  }).then(function(response) {
    // 304 now surfaces thanks to the explicit If-None-Match request header
    assert_equals(response.status, 304);
  });
}, "Testing conditional GET with ETags");
