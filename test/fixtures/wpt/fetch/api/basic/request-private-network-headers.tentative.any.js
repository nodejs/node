// META: global=window,worker
// META: script=../resources/utils.js

requestForbiddenHeaders(
    'Access-Control-Request-Private-Network is a forbidden request header',
    {'Access-Control-Request-Private-Network': ''});

var invalidRequestHeaders = [
  ["Access-Control-Request-Private-Network", "KO"],
];

invalidRequestHeaders.forEach(function(header) {
  test(function() {
    var request = new Request("");
    request.headers.set(header[0], header[1]);
    assert_equals(request.headers.get(header[0]), null);
  }, "Adding invalid request header \"" + header[0] + ": " + header[1] + "\"");
});
