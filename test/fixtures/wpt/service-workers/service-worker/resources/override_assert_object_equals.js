// .body attribute of Request and Response object are experimental feture. It is
// enabled when --enable-experimental-web-platform-features flag is set.
// Touching this attribute can change the behavior of the objects. To avoid
// touching it while comparing the objects in LayoutTest, we overwrite
// assert_object_equals method.

(function() {
  var original_assert_object_equals = self.assert_object_equals;
  function _brand(object) {
    return Object.prototype.toString.call(object).match(/^\[object (.*)\]$/)[1];
  }
  var assert_request_equals = function(actual, expected, prefix) {
    if (typeof actual !== 'object') {
      assert_equals(actual, expected, prefix);
      return;
    }
    assert_true(actual instanceof Request, prefix);
    assert_true(expected instanceof Request, prefix);
    assert_equals(actual.bodyUsed, expected.bodyUsed, prefix + '.bodyUsed');
    assert_equals(actual.method, expected.method, prefix + '.method');
    assert_equals(actual.url, expected.url, prefix + '.url');
    original_assert_object_equals(actual.headers, expected.headers,
                                  prefix + '.headers');
    assert_equals(actual.context, expected.context, prefix + '.context');
    assert_equals(actual.referrer, expected.referrer, prefix + '.referrer');
    assert_equals(actual.mode, expected.mode, prefix + '.mode');
    assert_equals(actual.credentials, expected.credentials,
                  prefix + '.credentials');
    assert_equals(actual.cache, expected.cache, prefix + '.cache');
  };
  var assert_response_equals = function(actual, expected, prefix) {
    if (typeof actual !== 'object') {
      assert_equals(actual, expected, prefix);
      return;
    }
    assert_true(actual instanceof Response, prefix);
    assert_true(expected instanceof Response, prefix);
    assert_equals(actual.bodyUsed, expected.bodyUsed, prefix + '.bodyUsed');
    assert_equals(actual.type, expected.type, prefix + '.type');
    assert_equals(actual.url, expected.url, prefix + '.url');
    assert_equals(actual.status, expected.status, prefix + '.status');
    assert_equals(actual.statusText, expected.statusText,
                  prefix + '.statusText');
    original_assert_object_equals(actual.headers, expected.headers,
                                  prefix + '.headers');
  };
  var assert_object_equals = function(actual, expected, description) {
    var prefix = (description ? description + ': ' : '') + _brand(expected);
    if (expected instanceof Request) {
      assert_request_equals(actual, expected, prefix);
    } else if (expected instanceof Response) {
      assert_response_equals(actual, expected, prefix);
    } else {
      original_assert_object_equals(actual, expected, description);
    }
  };
  self.assert_object_equals = assert_object_equals;
})();
