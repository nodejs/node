// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

/* Check preflight is ok if status is ok status (200  to 299)*/
function corsPreflightStatus(desc, corsUrl, preflightStatus) {
  var uuid_token = token();
  var url = corsUrl;
  var requestInit = {"mode": "cors"};
  /* Force preflight */
  requestInit["headers"] = {"x-force-preflight": ""};

  var urlParameters = "?token=" + uuid_token + "&max_age=0";
  urlParameters += "&allow_headers=x-force-preflight";
  urlParameters += "&preflight_status=" + preflightStatus;

  promise_test(function(test) {
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      assert_equals(resp.status, 200, "Clean stash response's status is 200");
      if (200 <= preflightStatus && 299 >= preflightStatus) {
        return fetch(url + urlParameters, requestInit).then(function(resp) {
          assert_equals(resp.status, 200, "Response's status is 200");
          assert_equals(resp.headers.get("x-did-preflight"), "1", "Preflight request has been made");
        });
      } else {
        return promise_rejects_js(test, TypeError, fetch(url + urlParameters, requestInit));
      }
    });
  }, desc);
}

var corsUrl = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "preflight.py";
for (status of [200, 201, 202, 203, 204, 205, 206,
                300, 301, 302, 303, 304, 305, 306, 307, 308,
                400, 401, 402, 403, 404, 405,
                501, 502, 503, 504, 505])
  corsPreflightStatus("Preflight answered with status " + status, corsUrl, status);
