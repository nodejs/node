// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

function corsCookies(desc, baseURL1, baseURL2, credentialsMode, cookies) {
  var urlSetCookie = baseURL1 + dirname(location.pathname) + RESOURCES_DIR + "top.txt";
  var urlCheckCookies = baseURL2 + dirname(location.pathname) + RESOURCES_DIR + "inspect-headers.py?cors&headers=cookie";
  //enable cors with credentials
  var urlParameters = "?pipe=header(Access-Control-Allow-Origin," + location.origin + ")";
  urlParameters += "|header(Access-Control-Allow-Credentials,true)";

  var urlCleanParameters = "?pipe=header(Access-Control-Allow-Origin," + location.origin + ")";
  urlCleanParameters += "|header(Access-Control-Allow-Credentials,true)";
  if (cookies) {
    urlParameters += "|header(Set-Cookie,";
    urlParameters += cookies.join(",True)|header(Set-Cookie,") +  ",True)";
    urlCleanParameters += "|header(Set-Cookie,";
    urlCleanParameters +=  cookies.join("%3B%20max-age=0,True)|header(Set-Cookie,") +  "%3B%20max-age=0,True)";
  }

  var requestInit = {"credentials": credentialsMode, "mode": "cors"};

  promise_test(function(test){
    return fetch(urlSetCookie + urlParameters, requestInit).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      //check cookies sent
      return fetch(urlCheckCookies, requestInit);
    }).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_false(resp.headers.has("Cookie") , "Cookie header is not exposed in response");
      if (credentialsMode === "include" && baseURL1 === baseURL2) {
        assert_equals(resp.headers.get("x-request-cookie") , cookies.join("; "), "Request includes cookie(s)");
      }
      else {
        assert_false(resp.headers.has("x-request-cookie") , "Request should have no cookie");
      }
      //clean cookies
      return fetch(urlSetCookie + urlCleanParameters, {"credentials": "include"});
    }).catch(function(e) {
      return fetch(urlSetCookie + urlCleanParameters, {"credentials": "include"}).then(function(resp) {
        throw e;
      })
    });
  }, desc);
}

var local = get_host_info().HTTP_ORIGIN;
var remote = get_host_info().HTTP_REMOTE_ORIGIN;
// FIXME: otherRemote might not be accessible on some test environments.
var otherRemote = local.replace("http://", "http://www.");

corsCookies("Omit mode: no cookie sent", local, local, "omit", ["g=7"]);
corsCookies("Include mode: 1 cookie", remote, remote, "include", ["a=1"]);
corsCookies("Include mode: local cookies are not sent with remote request", local, remote, "include", ["c=3"]);
corsCookies("Include mode: remote cookies are not sent with local request", remote, local, "include", ["d=4"]);
corsCookies("Same-origin mode: cookies are discarded in cors request", remote, remote, "same-origin", ["f=6"]);
corsCookies("Include mode: remote cookies are not sent with other remote request", remote, otherRemote, "include", ["e=5"]);
