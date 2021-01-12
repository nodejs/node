// META: script=/common/get-host-info.sub.js

var redirectLocation = "cors-top.txt";

function testRedirect(origin, redirectStatus, redirectMode, corsMode) {
  var url = new URL("../resources/redirect.py", self.location);
  if (origin === "cross-origin") {
    url.host = get_host_info().REMOTE_HOST;
    url.port = get_host_info().HTTP_PORT;
  }

  var urlParameters = "?redirect_status=" + redirectStatus;
  urlParameters += "&location=" + encodeURIComponent(redirectLocation);

  var requestInit = {redirect: redirectMode, mode: corsMode};

  promise_test(function(test) {
    if (redirectMode === "error" ||
        (corsMode === "no-cors" && redirectMode !== "follow" && origin !== "same-origin"))
      return promise_rejects_js(test, TypeError, fetch(url + urlParameters, requestInit));
    if (redirectMode === "manual")
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 0, "Response's status is 0");
        assert_equals(resp.type, "opaqueredirect", "Response's type is opaqueredirect");
        assert_equals(resp.statusText, "", "Response's statusText is \"\"");
        assert_equals(resp.url, url + urlParameters, "Response URL should be the original one");
      });
    if (redirectMode === "follow")
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        if (corsMode !== "no-cors" || origin === "same-origin") {
          assert_true(new URL(resp.url).pathname.endsWith(redirectLocation), "Response's url should be the redirected one");
          assert_equals(resp.status, 200, "Response's status is 200");
        } else {
          assert_equals(resp.type, "opaque", "Response is opaque");
        }
      });
    assert_unreached(redirectMode + " is no a valid redirect mode");
  }, origin + " redirect " + redirectStatus + " in " + redirectMode + " redirect and " + corsMode + " mode");
}

for (var origin of ["same-origin", "cross-origin"]) {
  for (var statusCode of [301, 302, 303, 307, 308]) {
    for (var redirect of ["error", "manual", "follow"]) {
      for (var mode of ["cors", "no-cors"])
        testRedirect(origin, statusCode, redirect, mode);
    }
  }
}

done();
