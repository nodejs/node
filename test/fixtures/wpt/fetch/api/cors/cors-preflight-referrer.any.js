// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function corsPreflightReferrer(desc, corsUrl, referrerPolicy, referrer, expectedReferrer) {
  var uuid_token = token();
  var url = corsUrl;
  var urlParameters = "?token=" + uuid_token + "&max_age=0";
  var requestInit = {"mode": "cors", "referrerPolicy": referrerPolicy};

  if (referrer)
      requestInit.referrer = referrer;

  /* Force preflight */
  requestInit["headers"] = {"x-force-preflight": ""};
  urlParameters += "&allow_headers=x-force-preflight";

  promise_test(function(test) {
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      assert_equals(resp.status, 200, "Clean stash response's status is 200");
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 200, "Response's status is 200");
        assert_equals(resp.headers.get("x-did-preflight"), "1", "Preflight request has been made");
        assert_equals(resp.headers.get("x-preflight-referrer"), expectedReferrer, "Preflight's referrer is correct");
        assert_equals(resp.headers.get("x-referrer"), expectedReferrer, "Request's referrer is correct");
        assert_equals(resp.headers.get("x-control-request-headers"), "", "Access-Control-Allow-Headers value");
      });
    });
  }, desc + " and referrer: " + (referrer ? "'" + referrer + "'" : "default"));
}

var corsUrl = get_host_info().HTTP_REMOTE_ORIGIN  + dirname(location.pathname) + RESOURCES_DIR + "preflight.py";
var origin = get_host_info().HTTP_ORIGIN + "/";

corsPreflightReferrer("Referrer policy: no-referrer", corsUrl, "no-referrer", undefined, "");
corsPreflightReferrer("Referrer policy: no-referrer", corsUrl, "no-referrer", "myreferrer", "");

corsPreflightReferrer("Referrer policy: \"\"", corsUrl, "", undefined, origin);
corsPreflightReferrer("Referrer policy: \"\"", corsUrl, "", "myreferrer", origin);

corsPreflightReferrer("Referrer policy: no-referrer-when-downgrade", corsUrl, "no-referrer-when-downgrade", undefined, location.toString())
corsPreflightReferrer("Referrer policy: no-referrer-when-downgrade", corsUrl, "no-referrer-when-downgrade", "myreferrer", new URL("myreferrer", location).toString());

corsPreflightReferrer("Referrer policy: origin", corsUrl, "origin", undefined, origin);
corsPreflightReferrer("Referrer policy: origin", corsUrl, "origin", "myreferrer", origin);

corsPreflightReferrer("Referrer policy: origin-when-cross-origin", corsUrl, "origin-when-cross-origin", undefined, origin);
corsPreflightReferrer("Referrer policy: origin-when-cross-origin", corsUrl, "origin-when-cross-origin", "myreferrer", origin);

corsPreflightReferrer("Referrer policy: unsafe-url", corsUrl, "unsafe-url", undefined, location.toString());
corsPreflightReferrer("Referrer policy: unsafe-url", corsUrl, "unsafe-url", "myreferrer", new URL("myreferrer", location).toString());
