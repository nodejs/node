// META: global=window,worker
// META: script=../resources/utils.js

function checkContentType(contentType, body)
{
    if (self.FormData && body instanceof self.FormData) {
        assert_true(contentType.startsWith("multipart/form-data; boundary="), "Request should have header content-type starting with multipart/form-data; boundary=, but got " + contentType);
        return;
    }

    var expectedContentType = "text/plain;charset=UTF-8";
    if(body === null || body instanceof ArrayBuffer || body.buffer instanceof ArrayBuffer)
        expectedContentType = null;
    else if (body instanceof Blob)
        expectedContentType = body.type ? body.type : null;
    else if (body instanceof URLSearchParams)
        expectedContentType = "application/x-www-form-urlencoded;charset=UTF-8";

    assert_equals(contentType , expectedContentType, "Request should have header content-type: " + expectedContentType);
}

function requestHeaders(desc, url, method, body, expectedOrigin, expectedContentLength) {
  var urlParameters = "?headers=origin|user-agent|accept-charset|content-length|content-type";
  var requestInit = {"method": method}
  promise_test(function(test){
    if (typeof body === "function")
      body = body();
    if (body)
      requestInit["body"] = body;
    return fetch(url + urlParameters, requestInit).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type , "basic", "Response's type is basic");
      assert_true(resp.headers.has("x-request-user-agent"), "Request has header user-agent");
      assert_false(resp.headers.has("accept-charset"), "Request has header accept-charset");
      assert_equals(resp.headers.get("x-request-origin") , expectedOrigin, "Request should have header origin: " + expectedOrigin);
      if (expectedContentLength !== undefined)
        assert_equals(resp.headers.get("x-request-content-length") , expectedContentLength, "Request should have header content-length: " + expectedContentLength);
      checkContentType(resp.headers.get("x-request-content-type"), body);
    });
  }, desc);
}

var url = RESOURCES_DIR + "inspect-headers.py"

requestHeaders("Fetch with GET", url, "GET", null, null, null);
requestHeaders("Fetch with HEAD", url, "HEAD", null, null, null);
requestHeaders("Fetch with PUT without body", url, "POST", null, location.origin, "0");
requestHeaders("Fetch with PUT with body", url, "PUT", "Request's body", location.origin, "14");
requestHeaders("Fetch with POST without body", url, "POST", null, location.origin, "0");
requestHeaders("Fetch with POST with text body", url, "POST", "Request's body", location.origin, "14");
requestHeaders("Fetch with POST with FormData body", url, "POST", function() { return new FormData(); }, location.origin);
requestHeaders("Fetch with POST with URLSearchParams body", url, "POST", function() { return new URLSearchParams("name=value"); }, location.origin, "10");
requestHeaders("Fetch with POST with Blob body", url, "POST", new Blob(["Test"]), location.origin, "4");
requestHeaders("Fetch with POST with ArrayBuffer body", url, "POST", new ArrayBuffer(4), location.origin, "4");
requestHeaders("Fetch with POST with Uint8Array body", url, "POST", new Uint8Array(4), location.origin, "4");
requestHeaders("Fetch with POST with Int8Array body", url, "POST", new Int8Array(4), location.origin, "4");
requestHeaders("Fetch with POST with Float16Array body", url, "POST", () => new Float16Array(1), location.origin, "2");
requestHeaders("Fetch with POST with Float32Array body", url, "POST", new Float32Array(1), location.origin, "4");
requestHeaders("Fetch with POST with Float64Array body", url, "POST", new Float64Array(1), location.origin, "8");
requestHeaders("Fetch with POST with DataView body", url, "POST", new DataView(new ArrayBuffer(8), 0, 4), location.origin, "4");
requestHeaders("Fetch with POST with Blob body with mime type", url, "POST", new Blob(["Test"], { type: "text/maybe" }), location.origin, "4");
requestHeaders("Fetch with Chicken", url, "Chicken", null, location.origin, null);
requestHeaders("Fetch with Chicken with body", url, "Chicken", "Request's body", location.origin, "14");

function requestOriginHeader(method, mode, needsOrigin) {
  promise_test(function(test){
    return fetch(url + "?headers=origin", {method:method, mode:mode}).then(function(resp) {
      assert_equals(resp.status, 200, "HTTP status is 200");
      assert_equals(resp.type , "basic", "Response's type is basic");
      if(needsOrigin)
        assert_equals(resp.headers.get("x-request-origin") , location.origin, "Request should have an Origin header with origin: " + location.origin);
      else
        assert_equals(resp.headers.get("x-request-origin"), null, "Request should not have an Origin header")
    });
  }, "Fetch with " + method + " and mode \"" + mode + "\" " + (needsOrigin ? "needs" : "does not need") + " an Origin header");
}

requestOriginHeader("GET", "cors", false);
requestOriginHeader("POST", "same-origin", true);
requestOriginHeader("POST", "no-cors", true);
requestOriginHeader("PUT", "same-origin", true);
requestOriginHeader("TacO", "same-origin", true);
requestOriginHeader("TacO", "cors", true);
