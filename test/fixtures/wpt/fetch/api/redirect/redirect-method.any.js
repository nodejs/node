// META: global=window,worker
// META: script=../resources/utils.js

// Creates a promise_test that fetches a URL that returns a redirect response.
//
// |opts| has additional options:
// |opts.body|: the request body as a string or blob (default is empty body)
// |opts.expectedBodyAsString|: the expected response body as a string. The
// server is expected to echo the request body. The default is the empty string
// if the request after redirection isn't POST; otherwise it's |opts.body|.
// |opts.expectedRequestContentType|: the expected Content-Type of redirected
// request.
function redirectMethod(desc, redirectUrl, redirectLocation, redirectStatus, method, expectedMethod, opts) {
  let url = redirectUrl;
  let urlParameters = "?redirect_status=" + redirectStatus;
  urlParameters += "&location=" + encodeURIComponent(redirectLocation);

  let requestHeaders = {
    "Content-Encoding": "Identity",
    "Content-Language": "en-US",
    "Content-Location": "foo",
  };
  let requestInit = {"method": method, "redirect": "follow", "headers" : requestHeaders};
  opts = opts || {};
  if (opts.body) {
    requestInit.body = opts.body;
  }

  promise_test(function(test) {
    return fetch(url + urlParameters, requestInit).then(function(resp) {
      let expectedRequestContentType = "NO";
      if (opts.expectedRequestContentType) {
        expectedRequestContentType = opts.expectedRequestContentType;
      }

      assert_equals(resp.status, 200, "Response's status is 200");
      assert_equals(resp.type, "basic", "Response's type basic");
      assert_equals(
        resp.headers.get("x-request-method"),
        expectedMethod,
        "Request method after redirection is " + expectedMethod);
      let hasRequestBodyHeader = true;
      if (opts.expectedStripRequestBodyHeader) {
        hasRequestBodyHeader = !opts.expectedStripRequestBodyHeader;
      }
      assert_equals(
        resp.headers.get("x-request-content-type"),
        expectedRequestContentType,
        "Request Content-Type after redirection is " + expectedRequestContentType);
      [
        "Content-Encoding",
        "Content-Language",
        "Content-Location"
      ].forEach(header => {
        let xHeader = "x-request-" + header.toLowerCase();
        let expectedValue = hasRequestBodyHeader ? requestHeaders[header] : "NO";
        assert_equals(
          resp.headers.get(xHeader),
          expectedValue,
          "Request " + header + " after redirection is " + expectedValue);
      });
      assert_true(resp.redirected);
      return resp.text().then(function(text) {
        let expectedBody = "";
        if (expectedMethod == "POST") {
          expectedBody = opts.expectedBodyAsString || requestInit.body;
        }
        let expectedContentLength = expectedBody ? expectedBody.length.toString() : "NO";
        assert_equals(text, expectedBody, "request body");
        assert_equals(
          resp.headers.get("x-request-content-length"),
          expectedContentLength,
          "Request Content-Length after redirection is " + expectedContentLength);
        });
    });
  }, desc);
}

promise_test(function(test) {
  assert_false(new Response().redirected);
  return fetch(RESOURCES_DIR + "method.py").then(function(resp) {
    assert_equals(resp.status, 200, "Response's status is 200");
    assert_false(resp.redirected);
  });
}, "Response.redirected should be false on not-redirected responses");

var redirUrl = RESOURCES_DIR + "redirect.py";
var locationUrl = "method.py";

const stringBody = "this is my body";
const blobBody = new Blob(["it's me the blob!", " ", "and more blob!"]);
const blobBodyAsString = "it's me the blob! and more blob!";

redirectMethod("Redirect 301 with GET", redirUrl, locationUrl, 301, "GET", "GET");
redirectMethod("Redirect 301 with POST", redirUrl, locationUrl, 301, "POST", "GET", { body: stringBody, expectedStripRequestBodyHeader: true });
redirectMethod("Redirect 301 with HEAD", redirUrl, locationUrl, 301, "HEAD", "HEAD");

redirectMethod("Redirect 302 with GET", redirUrl, locationUrl, 302, "GET", "GET");
redirectMethod("Redirect 302 with POST", redirUrl, locationUrl, 302, "POST", "GET", { body: stringBody, expectedStripRequestBodyHeader: true });
redirectMethod("Redirect 302 with HEAD", redirUrl, locationUrl, 302, "HEAD", "HEAD");

redirectMethod("Redirect 303 with GET", redirUrl, locationUrl, 303, "GET", "GET");
redirectMethod("Redirect 303 with POST", redirUrl, locationUrl, 303, "POST", "GET", { body: stringBody, expectedStripRequestBodyHeader: true });
redirectMethod("Redirect 303 with HEAD", redirUrl, locationUrl, 303, "HEAD", "HEAD");
redirectMethod("Redirect 303 with TESTING", redirUrl, locationUrl, 303, "TESTING", "GET", { expectedStripRequestBodyHeader: true });

redirectMethod("Redirect 307 with GET", redirUrl, locationUrl, 307, "GET", "GET");
redirectMethod("Redirect 307 with POST (string body)", redirUrl, locationUrl, 307, "POST", "POST", { body: stringBody , expectedRequestContentType: "text/plain;charset=UTF-8"});
redirectMethod("Redirect 307 with POST (blob body)", redirUrl, locationUrl, 307, "POST", "POST", { body: blobBody, expectedBodyAsString: blobBodyAsString });
redirectMethod("Redirect 307 with HEAD", redirUrl, locationUrl, 307, "HEAD", "HEAD");

done();
