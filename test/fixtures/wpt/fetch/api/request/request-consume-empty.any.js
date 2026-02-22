// META: global=window,worker
// META: title=Request consume empty bodies

function checkBodyText(test, request) {
  return request.text().then(function(bodyAsText) {
    assert_equals(bodyAsText, "", "Resolved value should be empty");
    assert_false(request.bodyUsed);
  });
}

async function checkBodyBlob(test, request) {
  const bodyAsBlob = await request.blob();
  const body = await bodyAsBlob.text();
  assert_equals(body, "", "Resolved value should be empty");
  assert_false(request.bodyUsed);
}

function checkBodyArrayBuffer(test, request) {
  return request.arrayBuffer().then(function(bodyAsArrayBuffer) {
    assert_equals(bodyAsArrayBuffer.byteLength, 0, "Resolved value should be empty");
    assert_false(request.bodyUsed);
  });
}

function checkBodyJSON(test, request) {
  return request.json().then(
    function(bodyAsJSON) {
      assert_unreached("JSON parsing should fail");
    },
    function() {
      assert_false(request.bodyUsed);
    });
}

function checkBodyFormData(test, request) {
  return request.formData().then(function(bodyAsFormData) {
    assert_true(bodyAsFormData instanceof FormData, "Should receive a FormData");
    assert_false(request.bodyUsed);
  });
}

function checkBodyFormDataError(test, request) {
  return promise_rejects_js(test, TypeError, request.formData()).then(function() {
    assert_false(request.bodyUsed);
  });
}

function checkRequestWithNoBody(bodyType, checkFunction, headers = []) {
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "headers": headers});
    assert_false(request.bodyUsed);
    return checkFunction(test, request);
  }, "Consume request's body as " + bodyType);
}

checkRequestWithNoBody("text", checkBodyText);
checkRequestWithNoBody("blob", checkBodyBlob);
checkRequestWithNoBody("arrayBuffer", checkBodyArrayBuffer);
checkRequestWithNoBody("json (error case)", checkBodyJSON);
checkRequestWithNoBody("formData with correct multipart type (error case)", checkBodyFormDataError, [["Content-Type", 'multipart/form-data; boundary="boundary"']]);
checkRequestWithNoBody("formData with correct urlencoded type", checkBodyFormData, [["Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"]]);
checkRequestWithNoBody("formData without correct type (error case)", checkBodyFormDataError);

function checkRequestWithEmptyBody(bodyType, body, asText) {
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "body": body});
    assert_false(request.bodyUsed, "bodyUsed is false at init");
    if (asText) {
      return request.text().then(function(bodyAsString) {
        assert_equals(bodyAsString.length, 0, "Resolved value should be empty");
        assert_true(request.bodyUsed, "bodyUsed is true after being consumed");
      });
    }
    return request.arrayBuffer().then(function(bodyAsArrayBuffer) {
      assert_equals(bodyAsArrayBuffer.byteLength, 0, "Resolved value should be empty");
      assert_true(request.bodyUsed, "bodyUsed is true after being consumed");
    });
  }, "Consume empty " + bodyType + " request body as " + (asText ? "text" : "arrayBuffer"));
}

// FIXME: Add BufferSource, FormData and URLSearchParams.
checkRequestWithEmptyBody("blob", new Blob([], { "type" : "text/plain" }), false);
checkRequestWithEmptyBody("text", "", false);
checkRequestWithEmptyBody("blob", new Blob([], { "type" : "text/plain" }), true);
checkRequestWithEmptyBody("text", "", true);
checkRequestWithEmptyBody("URLSearchParams", new URLSearchParams(""), true);
// FIXME: This test assumes that the empty string be returned but it is not clear whether that is right. See https://github.com/web-platform-tests/wpt/pull/3950.
checkRequestWithEmptyBody("FormData", new FormData(), true);
checkRequestWithEmptyBody("ArrayBuffer", new ArrayBuffer(), true);
