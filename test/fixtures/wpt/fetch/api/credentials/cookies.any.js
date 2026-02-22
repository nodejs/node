// META: global=window,worker
// META: script=../resources/utils.js

function cookies(desc, credentials1, credentials2 ,cookies) {
  var url = RESOURCES_DIR + "top.txt"
  var urlParameters = "";
  var urlCleanParameters = "";
  if (cookies) {
    urlParameters +="?pipe=header(Set-Cookie,";
    urlParameters += cookies.join(",True)|header(Set-Cookie,") +  ",True)";
    urlCleanParameters +="?pipe=header(Set-Cookie,";
    urlCleanParameters +=  cookies.join("%3B%20max-age=0,True)|header(Set-Cookie,") +  "%3B%20max-age=0,True)";
  }

  var requestInit = {"credentials": credentials1}
  promise_test(function(test){
    var requestInit = {"credentials": credentials1}
    return fetch(url + urlParameters, requestInit).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type , "basic", "Response's type is basic");
      //check cookies sent
      return fetch(RESOURCES_DIR + "inspect-headers.py?headers=cookie" , {"credentials": credentials2});
    }).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type , "basic", "Response's type is basic");
      assert_false(resp.headers.has("Cookie") , "Cookie header is not exposed in response");
      if (credentials1 != "omit" && credentials2 != "omit") {
        assert_equals(resp.headers.get("x-request-cookie") , cookies.join("; "), "Request include cookie(s)");
      }
      else {
        assert_false(resp.headers.has("x-request-cookie") , "Request does not have cookie(s)");
      }
      //clean cookies
      return fetch(url + urlCleanParameters, {"credentials": "include"});
    }).catch(function(e) {
      return fetch(url + urlCleanParameters, {"credentials": "include"}).then(function() {
         return Promise.reject(e);
      });
    });
  }, desc);
}

cookies("Include mode: 1 cookie", "include", "include", ["a=1"]);
cookies("Include mode: 2 cookies", "include", "include", ["b=2", "c=3"]);
cookies("Omit mode: discard cookies", "omit", "omit", ["d=4"]);
cookies("Omit mode: no cookie is stored", "omit", "include", ["e=5"]);
cookies("Omit mode: no cookie is sent", "include", "omit", ["f=6"]);
cookies("Same-origin mode: 1 cookie", "same-origin", "same-origin", ["a=1"]);
cookies("Same-origin mode: 2 cookies", "same-origin", "same-origin", ["b=2", "c=3"]);
