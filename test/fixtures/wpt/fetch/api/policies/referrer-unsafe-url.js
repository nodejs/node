if (this.document === undefined) {
  importScripts("/resources/testharness.js");
  importScripts("../resources/utils.js");

  // A nested importScripts() with a referrer-policy should have no effect
  // on overall worker policy.
  importScripts("nested-policy.js");
}

var referrerUrl = location.href;
var fetchedUrl = RESOURCES_DIR + "inspect-headers.py?headers=referer";

promise_test(function(test) {
  return fetch(fetchedUrl).then(function(resp) {
    assert_equals(resp.status, 200, "HTTP status is 200");
    assert_equals(resp.type , "basic", "Response's type is basic");
    assert_equals(resp.headers.get("x-request-referer"), referrerUrl, "request's referrer is " + referrerUrl);
  });
}, "Request's referrer is the full url of current document/worker");

done();
