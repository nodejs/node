// META: global=window,worker
// META: script=../resources/utils.js

function testReferrer(referrer, expected, desc) {
  promise_test(function(test) {
    var url = RESOURCES_DIR + "inspect-headers.py?headers=referer"
    var req = new Request(url, { referrer: referrer });
    return fetch(req).then(function(resp) {
      var actual = resp.headers.get("x-request-referer");
      if (expected) {
        assert_equals(actual, expected, "request's referer should be: " + expected);
        return;
      }
      if (actual) {
        assert_equals(actual, "", "request's referer should be empty");
      }
    });
  }, desc);
}

testReferrer("about:client", self.location.href, 'about:client referrer');

var fooURL = new URL("./foo", self.location).href;
testReferrer(fooURL, fooURL, 'url referrer');
