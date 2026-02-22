// META: global=window,worker
// META: title=Response consume empty bodies

function checkBodyText(test, response) {
  return response.text().then(function(bodyAsText) {
    assert_equals(bodyAsText, "", "Resolved value should be empty");
    assert_false(response.bodyUsed);
  });
}

async function checkBodyBlob(test, response) {
  const bodyAsBlob = await response.blob();
  const body = await bodyAsBlob.text();

  assert_equals(body, "", "Resolved value should be empty");
  assert_false(response.bodyUsed);
}

function checkBodyArrayBuffer(test, response) {
  return response.arrayBuffer().then(function(bodyAsArrayBuffer) {
    assert_equals(bodyAsArrayBuffer.byteLength, 0, "Resolved value should be empty");
    assert_false(response.bodyUsed);
  });
}

function checkBodyJSON(test, response) {
  return response.json().then(
    function(bodyAsJSON) {
      assert_unreached("JSON parsing should fail");
    },
    function() {
      assert_false(response.bodyUsed);
    });
}

function checkBodyFormData(test, response) {
  return response.formData().then(function(bodyAsFormData) {
    assert_true(bodyAsFormData instanceof FormData, "Should receive a FormData");
    assert_false(response.bodyUsed);
  });
}

function checkBodyFormDataError(test, response) {
  return promise_rejects_js(test, TypeError, response.formData()).then(function() {
    assert_false(response.bodyUsed);
  });
}

function checkResponseWithNoBody(bodyType, checkFunction, headers = []) {
  promise_test(function(test) {
    var response = new Response(undefined, { "headers": headers });
    assert_false(response.bodyUsed);
    return checkFunction(test, response);
  }, "Consume response's body as " + bodyType);
}

checkResponseWithNoBody("text", checkBodyText);
checkResponseWithNoBody("blob", checkBodyBlob);
checkResponseWithNoBody("arrayBuffer", checkBodyArrayBuffer);
checkResponseWithNoBody("json (error case)", checkBodyJSON);
checkResponseWithNoBody("formData with correct multipart type (error case)", checkBodyFormDataError, [["Content-Type", 'multipart/form-data; boundary="boundary"']]);
checkResponseWithNoBody("formData with correct urlencoded type", checkBodyFormData, [["Content-Type", "application/x-www-form-urlencoded;charset=UTF-8"]]);
checkResponseWithNoBody("formData without correct type (error case)", checkBodyFormDataError);

function checkResponseWithEmptyBody(bodyType, body, asText) {
  promise_test(function(test) {
    var response = new Response(body);
    assert_false(response.bodyUsed, "bodyUsed is false at init");
    if (asText) {
      return response.text().then(function(bodyAsString) {
        assert_equals(bodyAsString.length, 0, "Resolved value should be empty");
        assert_true(response.bodyUsed, "bodyUsed is true after being consumed");
      });
    }
    return response.arrayBuffer().then(function(bodyAsArrayBuffer) {
      assert_equals(bodyAsArrayBuffer.byteLength, 0, "Resolved value should be empty");
      assert_true(response.bodyUsed, "bodyUsed is true after being consumed");
    });
  }, "Consume empty " + bodyType + " response body as " + (asText ? "text" : "arrayBuffer"));
}

checkResponseWithEmptyBody("blob", new Blob([], { "type" : "text/plain" }), false);
checkResponseWithEmptyBody("text", "", false);
checkResponseWithEmptyBody("blob", new Blob([], { "type" : "text/plain" }), true);
checkResponseWithEmptyBody("text", "", true);
checkResponseWithEmptyBody("URLSearchParams", new URLSearchParams(""), true);
checkResponseWithEmptyBody("FormData", new FormData(), true);
checkResponseWithEmptyBody("ArrayBuffer", new ArrayBuffer(), true);
