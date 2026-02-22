// META: global=window,worker
// META: title=Request init: headers and body

test(function() {
  var headerDict = {"name1": "value1",
                    "name2": "value2",
                    "name3": "value3"
                    };
  var headers = new Headers(headerDict);
  var request = new Request("", { "headers" : headers })
  for (var name in headerDict) {
    assert_equals(request.headers.get(name), headerDict[name],
      "request's headers has " + name + " : " + headerDict[name]);
  }
}, "Initialize Request with headers values");

function makeRequestInit(body, method) {
  return {"method": method, "body": body};
}

function checkRequestInit(body, bodyType, expectedTextBody) {
  promise_test(function(test) {
    var request = new Request("", makeRequestInit(body, "POST"));
    if (body) {
      assert_throws_js(TypeError, function() { new Request("", makeRequestInit(body, "GET")); });
      assert_throws_js(TypeError, function() { new Request("", makeRequestInit(body, "HEAD")); });
    } else {
      new Request("", makeRequestInit(body, "GET")); // should not throw
    }
    var reqHeaders = request.headers;
    var mime = reqHeaders.get("Content-Type");
    assert_true(!body || (mime && mime.search(bodyType) > -1), "Content-Type header should be \"" + bodyType + "\", not \"" + mime + "\"");
    return request.text().then(function(bodyAsText) {
      //not equals: cannot guess formData exact value
      assert_true( bodyAsText.search(expectedTextBody) > -1, "Retrieve and verify request body");
    });
  }, `Initialize Request's body with "${body}", ${bodyType}`);
}

var blob = new Blob(["This is a blob"], {type: "application/octet-binary"});
var formaData = new FormData();
formaData.append("name", "value");
var usvString = "This is a USVString"

checkRequestInit(undefined, undefined, "");
checkRequestInit(null, null, "");
checkRequestInit(blob, "application/octet-binary", "This is a blob");
checkRequestInit(formaData, "multipart/form-data", "name=\"name\"\r\n\r\nvalue");
checkRequestInit(usvString, "text/plain;charset=UTF-8", "This is a USVString");
checkRequestInit({toString: () => "hi!"}, "text/plain;charset=UTF-8", "hi!");

// Ensure test does not time out in case of missing URLSearchParams support.
if (self.URLSearchParams) {
  var urlSearchParams = new URLSearchParams("name=value");
  checkRequestInit(urlSearchParams, "application/x-www-form-urlencoded;charset=UTF-8", "name=value");
} else {
  promise_test(function(test) {
    return Promise.reject("URLSearchParams not supported");
  }, "Initialize Request's body with application/x-www-form-urlencoded;charset=UTF-8");
}
