// META: global=window,worker
// META: title=Request consume
// META: script=../resources/utils.js

function checkBodyText(request, expectedBody) {
  return request.text().then(function(bodyAsText) {
    assert_equals(bodyAsText, expectedBody, "Retrieve and verify request's body");
    assert_true(request.bodyUsed, "body as text: bodyUsed turned true");
  });
}

async function checkBodyBlob(request, expectedBody, checkContentType) {
  const bodyAsBlob = await request.blob();

  if (checkContentType)
    assert_equals(bodyAsBlob.type, "text/plain", "Blob body type should be computed from the request Content-Type");

  const body = await bodyAsBlob.text();
  assert_equals(body, expectedBody, "Retrieve and verify request's body");
  assert_true(request.bodyUsed, "body as blob: bodyUsed turned true");
}

function checkBodyArrayBuffer(request, expectedBody) {
  return request.arrayBuffer().then(function(bodyAsArrayBuffer) {
    validateBufferFromString(bodyAsArrayBuffer, expectedBody, "Retrieve and verify request's body");
    assert_true(request.bodyUsed, "body as arrayBuffer: bodyUsed turned true");
  });
}

function checkBodyBytes(request, expectedBody) {
  return request.bytes().then(function(bodyAsUint8Array) {
    assert_true(bodyAsUint8Array instanceof Uint8Array);
    validateBufferFromString(bodyAsUint8Array.buffer, expectedBody, "Retrieve and verify request's body");
    assert_true(request.bodyUsed, "body as bytes: bodyUsed turned true");
  });
}

function checkBodyJSON(request, expectedBody) {
  return request.json().then(function(bodyAsJSON) {
    var strBody = JSON.stringify(bodyAsJSON)
    assert_equals(strBody, expectedBody, "Retrieve and verify request's body");
    assert_true(request.bodyUsed, "body as json: bodyUsed turned true");
  });
}

function checkBodyFormData(request, expectedBody) {
  return request.formData().then(function(bodyAsFormData) {
    assert_true(bodyAsFormData instanceof FormData, "Should receive a FormData");
    assert_true(request.bodyUsed, "body as formData: bodyUsed turned true");
  });
}

function checkRequestBody(body, expected, bodyType) {
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "body": body, "headers": [["Content-Type", "text/PLAIN"]] });
    assert_false(request.bodyUsed, "bodyUsed is false at init");
    return checkBodyText(request, expected);
  }, "Consume " + bodyType  + " request's body as text");
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "body": body });
    assert_false(request.bodyUsed, "bodyUsed is false at init");
    return checkBodyBlob(request, expected);
  }, "Consume " + bodyType  + " request's body as blob");
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "body": body });
    assert_false(request.bodyUsed, "bodyUsed is false at init");
    return checkBodyArrayBuffer(request, expected);
  }, "Consume " + bodyType  + " request's body as arrayBuffer");
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "body": body });
    assert_false(request.bodyUsed, "bodyUsed is false at init");
    return checkBodyBytes(request, expected);
  }, "Consume " + bodyType  + " request's body as bytes");
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "body": body });
    assert_false(request.bodyUsed, "bodyUsed is false at init");
    return checkBodyJSON(request, expected);
  }, "Consume " + bodyType  + " request's body as JSON");
}

var textData = JSON.stringify("This is response's body");
var blob = new Blob([textData], { "type" : "text/plain" });

checkRequestBody(textData, textData, "String");

var string = "\"123456\"";
function getArrayBuffer() {
    var arrayBuffer = new ArrayBuffer(8);
    var int8Array = new Int8Array(arrayBuffer);
    for (var cptr = 0; cptr < 8; cptr++)
        int8Array[cptr] = string.charCodeAt(cptr);
    return arrayBuffer;
}

function getArrayBufferWithZeros() {
    var arrayBuffer = new ArrayBuffer(10);
    var int8Array = new Int8Array(arrayBuffer);
    for (var cptr = 0; cptr < 8; cptr++)
        int8Array[cptr + 1] = string.charCodeAt(cptr);
    return arrayBuffer;
}

checkRequestBody(getArrayBuffer(), string, "ArrayBuffer");
checkRequestBody(new Uint8Array(getArrayBuffer()), string, "Uint8Array");
checkRequestBody(new Int8Array(getArrayBufferWithZeros(), 1, 8), string, "Int8Array");
checkRequestBody(new Float32Array(getArrayBuffer()), string, "Float32Array");
checkRequestBody(new DataView(getArrayBufferWithZeros(), 1, 8), string, "DataView");

promise_test(function(test) {
  var formData = new FormData();
  formData.append("name", "value")
  var request = new Request("", {"method": "POST", "body": formData });
  assert_false(request.bodyUsed, "bodyUsed is false at init");
  return checkBodyFormData(request, formData);
}, "Consume FormData request's body as FormData");

function checkBlobResponseBody(blobBody, blobData, bodyType, checkFunction) {
  promise_test(function(test) {
    var response = new Response(blobBody);
    assert_false(response.bodyUsed, "bodyUsed is false at init");
    return checkFunction(response, blobData);
  }, "Consume blob response's body as " + bodyType);
}

checkBlobResponseBody(blob, textData, "blob", checkBodyBlob);
checkBlobResponseBody(blob, textData, "text", checkBodyText);
checkBlobResponseBody(blob, textData, "json", checkBodyJSON);
checkBlobResponseBody(blob, textData, "arrayBuffer", checkBodyArrayBuffer);
checkBlobResponseBody(blob, textData, "bytes", checkBodyBytes);
checkBlobResponseBody(new Blob([""]), "", "blob (empty blob as input)", checkBodyBlob);

var goodJSONValues = ["null", "1", "true", "\"string\""];
goodJSONValues.forEach(function(value) {
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "body": value});
    return request.json().then(function(v) {
      assert_equals(v, JSON.parse(value));
    });
  }, "Consume JSON from text: '" + JSON.stringify(value) + "'");
});

var badJSONValues = ["undefined", "{", "a", "["];
badJSONValues.forEach(function(value) {
  promise_test(function(test) {
    var request = new Request("", {"method": "POST", "body": value});
    return promise_rejects_js(test, SyntaxError, request.json());
  }, "Trying to consume bad JSON text as JSON: '" + value + "'");
});
