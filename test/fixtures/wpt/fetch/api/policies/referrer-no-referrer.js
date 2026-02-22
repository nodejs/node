if (this.document === undefined) {
  importScripts("/resources/testharness.js");
  importScripts("../resources/utils.js");
}

var fetchedUrl = RESOURCES_DIR + "inspect-headers.py?headers=origin";

promise_test(function(test) {
  return fetch(fetchedUrl).then(function(resp) {
    assert_equals(resp.status, 200, "HTTP status is 200");
    assert_equals(resp.type , "basic", "Response's type is basic");
    var referrer = resp.headers.get("x-request-referer");
    //Either no referrer header is sent or it is empty
    if (referrer)
      assert_equals(referrer, "", "request's referrer is empty");
  });
}, "Request's referrer is empty");

done();
