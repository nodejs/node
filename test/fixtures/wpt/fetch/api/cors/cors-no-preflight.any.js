// META: script=/common/utils.js
// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function corsNoPreflight(desc, baseURL, method, headerName, headerValue) {

  var uuid_token = token();
  var url = baseURL + dirname(location.pathname) + RESOURCES_DIR + "preflight.py";
  var urlParameters = "?token=" + uuid_token + "&max_age=0";
  var requestInit = {"mode": "cors", "method": method, "headers":{}};
  if (headerName)
    requestInit["headers"][headerName] = headerValue;

  promise_test(function(test) {
    return fetch(RESOURCES_DIR + "clean-stash.py?token=" + uuid_token).then(function(resp) {
      assert_equals(resp.status, 200, "Clean stash response's status is 200");
      return fetch(url + urlParameters, requestInit).then(function(resp) {
        assert_equals(resp.status, 200, "Response's status is 200");
        assert_equals(resp.headers.get("x-did-preflight"), "0", "No preflight request has been made");
      });
    });
  }, desc);
}

var host_info = get_host_info();

corsNoPreflight("Cross domain basic usage [GET]", host_info.HTTP_REMOTE_ORIGIN, "GET");
corsNoPreflight("Same domain different port [GET]", host_info.HTTP_ORIGIN_WITH_DIFFERENT_PORT, "GET");
corsNoPreflight("Cross domain different port [GET]", host_info.HTTP_REMOTE_ORIGIN_WITH_DIFFERENT_PORT, "GET");
corsNoPreflight("Cross domain different protocol [GET]", host_info.HTTPS_REMOTE_ORIGIN, "GET");
corsNoPreflight("Same domain different protocol different port [GET]", host_info.HTTPS_ORIGIN, "GET");
corsNoPreflight("Cross domain [POST]", host_info.HTTP_REMOTE_ORIGIN, "POST");
corsNoPreflight("Cross domain [HEAD]", host_info.HTTP_REMOTE_ORIGIN, "HEAD");
corsNoPreflight("Cross domain [GET] [Accept: */*]", host_info.HTTP_REMOTE_ORIGIN, "GET" , "Accept", "*/*");
corsNoPreflight("Cross domain [GET] [Accept-Language: fr]", host_info.HTTP_REMOTE_ORIGIN, "GET" , "Accept-Language", "fr");
corsNoPreflight("Cross domain [GET] [Content-Language: fr]", host_info.HTTP_REMOTE_ORIGIN, "GET" , "Content-Language", "fr");
corsNoPreflight("Cross domain [GET] [Content-Type: application/x-www-form-urlencoded]", host_info.HTTP_REMOTE_ORIGIN, "GET" , "Content-Type", "application/x-www-form-urlencoded");
corsNoPreflight("Cross domain [GET] [Content-Type: multipart/form-data]", host_info.HTTP_REMOTE_ORIGIN, "GET" , "Content-Type", "multipart/form-data");
corsNoPreflight("Cross domain [GET] [Content-Type: text/plain]", host_info.HTTP_REMOTE_ORIGIN, "GET" , "Content-Type", "text/plain");
corsNoPreflight("Cross domain [GET] [Content-Type: text/plain;charset=utf-8]", host_info.HTTP_REMOTE_ORIGIN, "GET" , "Content-Type", "text/plain;charset=utf-8");
corsNoPreflight("Cross domain [GET] [Content-Type: Text/Plain;charset=utf-8]", host_info.HTTP_REMOTE_ORIGIN, "GET" , "Content-Type", "Text/Plain;charset=utf-8");
