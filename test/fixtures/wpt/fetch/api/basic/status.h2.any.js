// See also /xhr/status.h2.window.js

[
  200,
  210,
  400,
  404,
  410,
  500,
  502
].forEach(status => {
  promise_test(async t => {
    const response = await fetch("/xhr/resources/status.py?code=" + status);
    assert_equals(response.status, status, "status should be " + status);
    assert_equals(response.statusText, "", "statusText should be the empty string");
  }, "statusText over H2 for status " + status + " should be the empty string");
});
