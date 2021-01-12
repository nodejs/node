// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function corsRedirect(desc, redirectUrl, redirectLocation, redirectStatus, expectedOrigin) {
  var uuid_token = token();
  var url = redirectUrl;
  var urlParameters = "?token=" + uuid_token + "&max_age=0";
  urlParameters += "&redirect_status=" + redirectStatus;
  urlParameters += "&location=" + encodeURIComponent(redirectLocation);

  var requestInit = {"mode": "cors", "redirect": "follow"};

  return promise_test(function(test) {
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 200, "Response's status is 200");
        assert_equals(resp.headers.get("x-did-preflight"), "0", "No preflight request has been made");
        assert_equals(resp.headers.get("x-origin"), expectedOrigin, "Origin is correctly set after redirect");
      });
    });
  }, desc);
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
  corsRedirect("Redirect " + code + ": cors to same cors", remoteRedirect, remoteLocation, code, location.origin);
  corsRedirect("Redirect " + code + ": cors to another cors", remoteRedirect, remoteLocation2, code, "null");
  corsRedirect("Redirect " + code + ": same origin to cors", localRedirect, remoteLocation, code, location.origin);
  corsRedirect("Redirect " + code + ": cors to same origin", remoteRedirect, localLocation, code, "null");
}
