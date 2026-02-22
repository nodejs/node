// META: script=../resources/utils.js
// META: script=/common/get-host-info.sub.js

const url = get_host_info().HTTP_REMOTE_ORIGIN + dirname(location.pathname) + RESOURCES_DIR + "preflight.py",
      origin = location.origin // assuming an ASCII origin

function preflightTest(succeeds, withCredentials, allowMethod, allowHeader, useMethod, useHeader) {
  return promise_test(t => {
    let testURL = url + "?",
        requestInit = {}
    if (withCredentials) {
      testURL += "origin=" + origin + "&"
      testURL += "credentials&"
      requestInit.credentials = "include"
    }
    if (useMethod) {
      requestInit.method = useMethod
    }
    if (useHeader.length > 0) {
      requestInit.headers = [useHeader]
    }
    testURL += "allow_methods=" + allowMethod + "&"
    testURL += "allow_headers=" + allowHeader + "&"

    if (succeeds) {
      return fetch(testURL, requestInit).then(resp => {
        assert_equals(resp.headers.get("x-origin"), origin)
      })
    } else {
      return promise_rejects_js(t, TypeError, fetch(testURL, requestInit))
    }
  }, "CORS that " + (succeeds ? "succeeds" : "fails") + " with credentials: " + withCredentials + "; method: " + useMethod + " (allowed: " + allowMethod + "); header: " + useHeader + " (allowed: " + allowHeader + ")")
}

// "GET" does not pass the case-sensitive method check, but in the safe list.
preflightTest(true, false, "get", "x-test", "GET", ["X-Test", "1"])
// Headers check is case-insensitive, and "*" works as any for method.
preflightTest(true, false, "*", "x-test", "SUPER", ["X-Test", "1"])
// "*" works as any only without credentials.
preflightTest(true, false, "*", "*", "OK", ["X-Test", "1"])
preflightTest(false, true, "*", "*", "OK", ["X-Test", "1"])
preflightTest(false, true, "*", "", "PUT", [])
preflightTest(false, true, "get", "*", "GET", ["X-Test", "1"])
preflightTest(false, true, "*", "*", "GET", ["X-Test", "1"])
// Exact character match works even for "*" with credentials.
preflightTest(true, true, "*", "*", "*", ["*", "1"])

// The following methods are upper-cased for init["method"] by
// https://fetch.spec.whatwg.org/#concept-method-normalize
// but not in Access-Control-Allow-Methods response.
// But they are https://fetch.spec.whatwg.org/#cors-safelisted-method,
// CORS anyway passes regardless of the cases.
for (const METHOD of ['GET', 'HEAD', 'POST']) {
  const method = METHOD.toLowerCase();
  preflightTest(true, true, METHOD, "*", METHOD, [])
  preflightTest(true, true, METHOD, "*", method, [])
  preflightTest(true, true, method, "*", METHOD, [])
  preflightTest(true, true, method, "*", method, [])
}

// The following methods are upper-cased for init["method"] by
// https://fetch.spec.whatwg.org/#concept-method-normalize
// but not in Access-Control-Allow-Methods response.
// As they are not https://fetch.spec.whatwg.org/#cors-safelisted-method,
// Access-Control-Allow-Methods should contain upper-cased methods,
// while init["method"] can be either in upper or lower case.
for (const METHOD of ['DELETE', 'PUT']) {
  const method = METHOD.toLowerCase();
  preflightTest(true, true, METHOD, "*", METHOD, [])
  preflightTest(true, true, METHOD, "*", method, [])
  preflightTest(false, true, method, "*", METHOD, [])
  preflightTest(false, true, method, "*", method, [])
}

// "PATCH" is NOT upper-cased in both places because it is not listed in
// https://fetch.spec.whatwg.org/#concept-method-normalize.
// So Access-Control-Allow-Methods value and init["method"] should match
// case-sensitively.
preflightTest(true, true, "PATCH", "*", "PATCH", [])
preflightTest(false, true, "PATCH", "*", "patch", [])
preflightTest(false, true, "patch", "*", "PATCH", [])
preflightTest(true, true, "patch", "*", "patch", [])

// "Authorization" header can't be wildcarded.
preflightTest(false, false, "*", "*", "POST", ["Authorization", "123"])
preflightTest(true, false, "*", "*, Authorization", "POST", ["Authorization", "123"])
