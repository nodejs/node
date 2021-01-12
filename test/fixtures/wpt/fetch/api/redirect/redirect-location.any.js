// META: global=window,worker
// META: script=../resources/utils.js

function redirectLocation(desc, redirectUrl, redirectLocation, redirectStatus, redirectMode, shouldPass) {
  var url = redirectUrl;
  var urlParameters = "?redirect_status=" + redirectStatus;
  if (redirectLocation)
    urlParameters += "&location=" + encodeURIComponent(redirectLocation);

  var requestInit = {"redirect": redirectMode};

  promise_test(function(test) {
    if (redirectMode === "error" || !shouldPass)
      return promise_rejects_js(test, TypeError, fetch(url + urlParameters, requestInit));
    if (redirectMode === "manual")
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 0, "Response's status is 0");
        assert_equals(resp.type, "opaqueredirect", "Response's type is opaqueredirect");
        assert_equals(resp.statusText, "", "Response's statusText is \"\"");
        assert_true(resp.headers.entries().next().done, "Headers should be empty");
      });

    if (redirectMode === "follow")
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, redirectStatus, "Response's status is " + redirectStatus);
      });
    assert_unreached(redirectMode + " is not a valid redirect mode");
  }, desc);
}

var redirUrl = RESOURCES_DIR + "redirect.py";
var locationUrl = "top.txt";
var invalidLocationUrl = "invalidurl:";
var dataLocationUrl = "data:,data%20url";
// FIXME: We may want to mix redirect-mode and cors-mode.
// FIXME: Add tests for "error" redirect-mode.
for (var statusCode of [301, 302, 303, 307, 308]) {
  redirectLocation("Redirect " + statusCode + " in \"follow\" mode without location", redirUrl, undefined, statusCode, "follow", true);
  redirectLocation("Redirect " + statusCode + " in \"manual\" mode without location", redirUrl, undefined, statusCode, "manual", true);

  redirectLocation("Redirect " + statusCode + " in \"follow\" mode with invalid location", redirUrl, invalidLocationUrl, statusCode, "follow", false);
  redirectLocation("Redirect " + statusCode + " in \"manual\" mode with invalid location", redirUrl, invalidLocationUrl, statusCode, "manual", true);

  redirectLocation("Redirect " + statusCode + " in \"follow\" mode with data location", redirUrl, dataLocationUrl, statusCode, "follow", false);
  redirectLocation("Redirect " + statusCode + " in \"manual\" mode with data location", redirUrl, dataLocationUrl, statusCode, "manual", true);
}

done();
