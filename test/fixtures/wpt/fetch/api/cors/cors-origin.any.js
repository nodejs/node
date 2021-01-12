// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

/* If origin is undefined, it is set to fetched url's origin*/
function corsOrigin(desc, baseURL, method, origin, shouldPass) {
  if (!origin)
    origin = baseURL;

  var uuid_token = token();
  var urlParameters = "?token=" + uuid_token + "&max_age=0&origin=" + encodeURIComponent(origin) + "&allow_methods=" + method;
  var url = baseURL + dirname(location.pathname) + RESOURCES_DIR + "preflight.py";
  var requestInit = {"mode": "cors", "method": method};

  promise_test(function(test) {
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      assert_equals(resp.status, 200, "Clean stash response's status is 200");
      if (shouldPass) {
        return fetch(url + urlParameters, requestInit).then(function(resp) {
          assert_equals(resp.status, 200, "Response's status is 200");
        });
      } else {
        return promise_rejects_js(test, TypeError, fetch(url + urlParameters, requestInit));
      }
    });
  }, desc);

}

var host_info = get_host_info();

/* Actual origin */
var origin = host_info.HTTP_ORIGIN;

corsOrigin("Cross domain different subdomain [origin OK]", host_info.HTTP_REMOTE_ORIGIN, "GET", origin, true);
corsOrigin("Cross domain different subdomain [origin KO]", host_info.HTTP_REMOTE_ORIGIN, "GET", undefined, false);
corsOrigin("Same domain different port [origin OK]", host_info.HTTP_ORIGIN_WITH_DIFFERENT_PORT, "GET", origin, true);
corsOrigin("Same domain different port [origin KO]", host_info.HTTP_ORIGIN_WITH_DIFFERENT_PORT, "GET", undefined, false);
corsOrigin("Cross domain different port [origin OK]", host_info.HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT, "GET", origin, true);
corsOrigin("Cross domain different port [origin KO]", host_info.HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT, "GET", undefined, false);
corsOrigin("Cross domain different protocol [origin OK]", host_info.HTTPS_REMOTE_ORIGIN, "GET", origin, true);
corsOrigin("Cross domain different protocol [origin KO]", host_info.HTTPS_REMOTE_ORIGIN, "GET", undefined, false);
corsOrigin("Same domain different protocol different port [origin OK]", host_info.HTTPS_ORIGIN, "GET", origin, true);
corsOrigin("Same domain different protocol different port [origin KO]", host_info.HTTPS_ORIGIN, "GET", undefined, false);
corsOrigin("Cross domain [POST] [origin OK]", host_info.HTTP_REMOTE_ORIGIN, "POST", origin, true);
corsOrigin("Cross domain [POST] [origin KO]", host_info.HTTP_REMOTE_ORIGIN, "POST", undefined, false);
corsOrigin("Cross domain [HEAD] [origin OK]", host_info.HTTP_REMOTE_ORIGIN, "HEAD", origin, true);
corsOrigin("Cross domain [HEAD] [origin KO]", host_info.HTTP_REMOTE_ORIGIN, "HEAD", undefined, false);
corsOrigin("CORS preflight [PUT] [origin OK]", host_info.HTTP_REMOTE_ORIGIN, "PUT", origin, true);
corsOrigin("CORS preflight [PUT] [origin KO]", host_info.HTTP_REMOTE_ORIGIN, "PUT", undefined, false);
corsOrigin("Allowed origin: \"\" [origin KO]", host_info.HTTP_REMOTE_ORIGIN, "GET", "" , false);
