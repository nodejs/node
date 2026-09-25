importScripts("/resources/testharness.js");

var expected = ['XMLHttpRequest', 'WebSocket', 'EventSource', 'MessageChannel', 'Worker'];
for (var i = 0; i < expected.length; ++i) {
  var property = expected[i];
  test(function() {
    assert_true(property in self);
  }, "existence of " + property);
}

done();
