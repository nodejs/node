if (this.document === undefined) {
  importScripts("/resources/testharness.js");
  importScripts("../resources/utils.js");

  // A nested importScripts() with a referrer-policy should have no effect
  // on overall worker policy.
  importScripts("nested-policy.js");
}

var referrerOrigin = (new URL("/", location.href)).href;
var fetchedUrl = RESOURCES_DIR + "inspect-headers.py?headers=referer";

promise_test(function(test) {
  return fetch(fetchedUrl).then(function(resp) {
    assert_equals(resp.status, 200, "HTTP status is 200");
    assert_equals(resp.type , "basic", "Response's type is basic");
    assert_equals(resp.headers.get("x-request-referer"), referrerOrigin, "request's referrer is " + referrerOrigin);
  });
}, "Request's referrer is origin");

promise_test(function(test) {
  var referrerUrl = "https://{{domains[www]}}:{{ports[https][0]}}/";
  return fetch(fetchedUrl, { "referrer": referrerUrl }).then(function(resp) {
    assert_equals(resp.status, 200, "HTTP status is 200");
    assert_equals(resp.type , "basic", "Response's type is basic");
    assert_equals(resp.headers.get("x-request-referer"), referrerOrigin, "request's referrer is " + referrerOrigin);
  });
}, "Cross-origin referrer is overridden by client origin");

done();
