// META: global=window,worker
// META: title=Response init: body and headers
// META: script=../resources/utils.js

test(function() {
  var headerDict = {"name1": "value1",
                    "name2": "value2",
                    "name3": "value3"
  };
  var headers = new Headers(headerDict);
  var response = new Response("", { "headers" : headers })
  for (var name in headerDict) {
    assert_equals(response.headers.get(name), headerDict[name],
      "response's headers has " + name + " : " + headerDict[name]);
  }
}, "Initialize Response with headers values");

function checkResponseInit(body, bodyType, expectedTextBody) {
  promise_test(function(test) {
    var response = new Response(body);
    var resHeaders = response.headers;
    var mime = resHeaders.get("Content-Type");
    assert_true(mime && mime.search(bodyType) > -1, "Content-Type header should be \"" + bodyType + "\" ");
    return response.text().then(function(bodyAsText) {
      //not equals: cannot guess formData exact value
      assert_true(bodyAsText.search(expectedTextBody) > -1, "Retrieve and verify response body");
    });
  }, "Initialize Response's body with " + bodyType);
}

var blob = new Blob(["This is a blob"], {type: "application/octet-binary"});
var formaData = new FormData();
formaData.append("name", "value");
var urlSearchParams = "URLSearchParams are not supported";
//avoid test timeout if not implemented
if (self.URLSearchParams)
  urlSearchParams = new URLSearchParams("name=value");
var usvString = "This is a USVString"

checkResponseInit(blob, "application/octet-binary", "This is a blob");
checkResponseInit(formaData, "multipart/form-data", "name=\"name\"\r\n\r\nvalue");
checkResponseInit(urlSearchParams, "application/x-www-form-urlencoded;charset=UTF-8", "name=value");
checkResponseInit(usvString, "text/plain;charset=UTF-8", "This is a USVString");

promise_test(function(test) {
  var body = "This is response body";
  var response = new Response(body);
  return validateStreamFromString(response.body.getReader(), body);
}, "Read Response's body as readableStream");

promise_test(function(test) {
  var response = new Response("This is my fork", {"headers" : [["Content-Type", ""]]});
  return response.blob().then(function(blob) {
    assert_equals(blob.type, "", "Blob type should be the empty string");
  });
}, "Testing empty Response Content-Type header");

test(function() {
  var response = new Response(null, {status: 204});
  assert_equals(response.body, null);
}, "Testing null Response body");
