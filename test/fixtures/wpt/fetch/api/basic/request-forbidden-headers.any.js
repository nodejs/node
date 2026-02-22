// META: global=window,worker
// META: script=../resources/utils.js

function requestValidOverrideHeaders(desc, validHeaders) {
  var url = RESOURCES_DIR + "inspect-headers.py";
  var requestInit = {"headers": validHeaders}
  var urlParameters = "?headers=" + Object.keys(validHeaders).join("|");

  promise_test(function(test){
    return fetch(url + urlParameters, requestInit).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type , "basic", "Response's type is basic");
      for (var header in validHeaders)
        assert_equals(resp.headers.get("x-request-" + header), validHeaders[header], header + "is not skipped for non-forbidden methods");
    });
  }, desc);
}

requestForbiddenHeaders("Accept-Charset is a forbidden request header", {"Accept-Charset": "utf-8"});
requestForbiddenHeaders("Accept-Encoding is a forbidden request header", {"Accept-Encoding": ""});

requestForbiddenHeaders("Access-Control-Request-Headers is a forbidden request header", {"Access-Control-Request-Headers": ""});
requestForbiddenHeaders("Access-Control-Request-Method is a forbidden request header", {"Access-Control-Request-Method": ""});
requestForbiddenHeaders("Connection is a forbidden request header", {"Connection": "close"});
requestForbiddenHeaders("Content-Length is a forbidden request header", {"Content-Length": "42"});
requestForbiddenHeaders("Cookie is a forbidden request header", {"Cookie": "cookie=none"});
requestForbiddenHeaders("Cookie2 is a forbidden request header", {"Cookie2": "cookie2=none"});
requestForbiddenHeaders("Date is a forbidden request header", {"Date": "Wed, 04 May 1988 22:22:22 GMT"});
requestForbiddenHeaders("DNT is a forbidden request header", {"DNT": "4"});
requestForbiddenHeaders("Expect is a forbidden request header", {"Expect": "100-continue"});
requestForbiddenHeaders("Host is a forbidden request header", {"Host": "http://wrong-host.com"});
requestForbiddenHeaders("Keep-Alive is a forbidden request header", {"Keep-Alive": "timeout=15"});
requestForbiddenHeaders("Origin is a forbidden request header", {"Origin": "http://wrong-origin.com"});
requestForbiddenHeaders("Referer is a forbidden request header", {"Referer": "http://wrong-referer.com"});
requestForbiddenHeaders("TE is a forbidden request header", {"TE": "trailers"});
requestForbiddenHeaders("Trailer is a forbidden request header", {"Trailer": "Accept"});
requestForbiddenHeaders("Transfer-Encoding is a forbidden request header", {"Transfer-Encoding": "chunked"});
requestForbiddenHeaders("Upgrade is a forbidden request header", {"Upgrade": "HTTP/2.0"});
requestForbiddenHeaders("Via is a forbidden request header", {"Via": "1.1 nowhere.com"});
requestForbiddenHeaders("Proxy- is a forbidden request header", {"Proxy-": "value"});
requestForbiddenHeaders("Proxy-Test is a forbidden request header", {"Proxy-Test": "value"});
requestForbiddenHeaders("Sec- is a forbidden request header", {"Sec-": "value"});
requestForbiddenHeaders("Sec-Test is a forbidden request header", {"Sec-Test": "value"});

let forbiddenMethods = [
  "TRACE",
  "TRACK",
  "CONNECT",
  "trace",
  "track",
  "connect",
  "trace,",
  "GET,track ",
  " connect",
];

let overrideHeaders = [
  "x-http-method-override",
  "x-http-method",
  "x-method-override",
  "X-HTTP-METHOD-OVERRIDE",
  "X-HTTP-METHOD",
  "X-METHOD-OVERRIDE",
];

for (forbiddenMethod of forbiddenMethods) {
    for (overrideHeader of overrideHeaders) {
       requestForbiddenHeaders(`header ${overrideHeader} is forbidden to use value ${forbiddenMethod}`, {[overrideHeader]: forbiddenMethod});
    }
}

let permittedValues = [
  "GETTRACE",
  "GET",
  "\",TRACE\",",
];

for (permittedValue of permittedValues) {
    for (overrideHeader of overrideHeaders) {
       requestValidOverrideHeaders(`header ${overrideHeader} is allowed to use value ${permittedValue}`, {[overrideHeader]: permittedValue});
    }
}
