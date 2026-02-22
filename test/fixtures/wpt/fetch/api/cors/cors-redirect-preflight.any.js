// META: timeout=long
// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function corsRedirect(desc, redirectUrl, redirectLocation, redirectStatus, expectSuccess) {
  var urlBaseParameters = "&redirect_status=" + redirectStatus;
  var urlParametersSuccess = urlBaseParameters + "&allow_headers=x-w3c&location=" + encodeURIComponent(redirectLocation + "?allow_headers=x-w3c");
  var urlParametersFailure = urlBaseParameters + "&location=" + encodeURIComponent(redirectLocation);

  var requestInit = {"mode": "cors", "redirect": "follow", "headers" : [["x-w3c", "test"]]};

  promise_test(function(test) {
    var uuid_token = token();
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      return fetch(redirectUrl + "?token=" + uuid_token + "&max_age=0" + urlParametersSuccess, requestInit).then(function(resp) {
        assert_equals(resp.status, 200, "Response's status is 200");
        assert_equals(resp.headers.get("x-did-preflight"), "1", "Preflight request has been made");
      });
    });
  }, desc + " (preflight after redirection success case)");
  promise_test(function(test) {
    var uuid_token = token();
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      return promise_rejects_js(test, TypeError, fetch(redirectUrl + "?token=" + uuid_token + "&max_age=0" + urlParametersFailure, requestInit));
    });
  }, desc + " (preflight after redirection failure case)");
}

var redirPath = dirname(location.pathname) + RESOURCES_DIR + "redirect.py";
var preflightPath = dirname(location.pathname) + RESOURCES_DIR + "preflight.py";

var host_info = get_host_info();

var localRedirect = host_info.HTTP_ORIGIN + redirPath;
var remoteRedirect = host_info.HTTP_REMOTE_ORIGIN + redirPath;

var localLocation = host_info.HTTP_ORIGIN + preflightPath;
var remoteLocation = host_info.HTTP_REMOTE_ORIGIN + preflightPath;
var remoteLocation2 = host_info.HTTP_ORIGIN_WITH_DIFFERENT_PORT + preflightPath;

for (var code of [301, 302, 303, 307, 308]) {
  corsRedirect("Redirect " + code + ": same origin to cors", localRedirect, remoteLocation, code);
  corsRedirect("Redirect " + code + ": cors to same origin", remoteRedirect, localLocation, code);
  corsRedirect("Redirect " + code + ": cors to another cors", remoteRedirect, remoteLocation2, code);
}
