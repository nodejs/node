// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function corsPreflightResponseValidation(desc, corsUrl, allowHeaders, allowMethods) {
  var uuid_token = token();
  var url = corsUrl;
  var requestInit = {"mode": "cors"};
  /* Force preflight */
  requestInit["headers"] = {"x-force-preflight": ""};

  var urlParameters = "?token=" + uuid_token + "&max_age=0";
  urlParameters += "&allow_headers=x-force-preflight";
  if (allowHeaders)
    urlParameters += "," + allowHeaders;
  if (allowMethods)
    urlParameters += "&allow_methods="+ allowMethods;

  promise_test(function(test) {
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(async function(resp) {
      assert_equals(resp.status, 200, "Clean stash response's status is 200");
      await promise_rejects_js(test, TypeError, fetch(url + urlParameters, requestInit));

      return fetch(url + urlParameters).then(function(resp) {
        assert_equals(resp.headers.get("x-did-preflight"), "1", "Preflight request has been made");
      });
    });
  }, desc);
}

var corsUrl = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "preflight.py";
corsPreflightResponseValidation("Preflight response with a bad Access-Control-Allow-Headers", corsUrl, "Bad value", null);
corsPreflightResponseValidation("Preflight response with a bad Access-Control-Allow-Methods", corsUrl, null, "Bad value");
