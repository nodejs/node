// META: global=window,worker
// META: script=../resources/utils.js
// META: script=/common/utils.js
// META: timeout=long

function redirectCount(desc, redirectUrl, redirectLocation, redirectStatus, maxCount, shouldPass) {
  var uuid_token = token();

  var urlParameters = "?token=" + uuid_token + "&max_age=0";
  urlParameters += "&redirect_status=" + redirectStatus;
  urlParameters += "&max_count=" + maxCount;
  if (redirectLocation)
    urlParameters += "&location=" + encodeURIComponent(redirectLocation);

  var url = redirectUrl;
  var requestInit = {"redirect": "follow"};

  promise_test(function(test) {
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      assert_equals(resp.status, 200, "Clean stash response's status is 200");

      if (!shouldPass)
        return promise_rejects_js(test, TypeError, fetch(url + urlParameters, requestInit));

      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 200, "Response's status is 200");
        return resp.text();
      }).then(function(body) {
        assert_equals(body, maxCount.toString(), "Redirected " + maxCount + " times");
      });
    });
  }, desc);
}

var redirUrl = RESOURCES_DIR + "redirect.py";

for (var statusCode of [301, 302, 303, 307, 308]) {
  redirectCount("Redirect " + statusCode + " 20 times", redirUrl, redirUrl, statusCode, 20, true);
  redirectCount("Redirect " + statusCode + " 21 times", redirUrl, redirUrl, statusCode, 21, false);
}

done();
