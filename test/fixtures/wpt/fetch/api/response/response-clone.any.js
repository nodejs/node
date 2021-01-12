// META: global=window,worker
// META: title=Response clone
// META: script=../resources/utils.js

var defaultValues = { "type" : "default",
                      "url" : "",
                      "ok" : true,
                      "status" : 200,
                      "statusText" : ""
};

var response = new Response();
var clonedResponse = response.clone();
test(function() {
  for (var attributeName in defaultValues) {
      var expectedValue = defaultValues[attributeName];
      assert_equals(clonedResponse[attributeName], expectedValue,
        "Expect default response." + attributeName + " is " + expectedValue);
  }
}, "Check Response's clone with default values, without body");

var body = "This is response body";
var headersInit = { "name" : "value" };
var responseInit  = { "status" : 200,
                      "statusText" : "GOOD",
                      "headers" : headersInit
};
var response = new Response(body, responseInit);
var clonedResponse = response.clone();
test(function() {
  assert_equals(clonedResponse.status, responseInit["status"],
    "Expect response.status is " + responseInit["status"]);
  assert_equals(clonedResponse.statusText, responseInit["statusText"],
    "Expect response.statusText is " + responseInit["statusText"]);
  assert_equals(clonedResponse.headers.get("name"), "value",
    "Expect response.headers has name:value header");
}, "Check Response's clone has the expected attribute values");

promise_test(function(test) {
  return validateStreamFromString(response.body.getReader(), body);
}, "Check orginal response's body after cloning");

promise_test(function(test) {
  return validateStreamFromString(clonedResponse.body.getReader(), body);
}, "Check cloned response's body");

promise_test(function(test) {
  var disturbedResponse = new Response("data");
  return disturbedResponse.text().then(function() {
      assert_true(disturbedResponse.bodyUsed, "response is disturbed");
      assert_throws_js(TypeError, function() { disturbedResponse.clone(); },
        "Expect TypeError exception");
  });
}, "Cannot clone a disturbed response");

promise_test(function(t) {
    var clone;
    var result;
    var response;
    return fetch('../resources/trickle.py?count=2&delay=100').then(function(res) {
        clone = res.clone();
        response = res;
        return clone.text();
    }).then(function(r) {
        assert_equals(r.length, 26);
        result = r;
        return response.text();
    }).then(function(r) {
        assert_equals(r, result, "cloned responses should provide the same data");
    });
  }, 'Cloned responses should provide the same data');

promise_test(function(t) {
    var clone;
    return fetch('../resources/trickle.py?count=2&delay=100').then(function(res) {
        clone = res.clone();
        res.body.cancel();
        assert_true(res.bodyUsed);
        assert_false(clone.bodyUsed);
        return clone.arrayBuffer();
    }).then(function(r) {
        assert_equals(r.byteLength, 26);
        assert_true(clone.bodyUsed);
    });
}, 'Cancelling stream should not affect cloned one');

function testReadableStreamClone(initialBuffer, bufferType)
{
    promise_test(function(test) {
        var response = new Response(new ReadableStream({start : function(controller) {
            controller.enqueue(initialBuffer);
            controller.close();
        }}));

        var clone = response.clone();
        var stream1 = response.body;
        var stream2 = clone.body;

        var buffer;
        return stream1.getReader().read().then(function(data) {
            assert_false(data.done);
            assert_equals(data.value, initialBuffer, "Buffer of being-cloned response stream is the same as the original buffer");
            return stream2.getReader().read();
        }).then(function(data) {
            assert_false(data.done);
            assert_array_equals(data.value, initialBuffer, "Cloned buffer chunks have the same content");
            assert_equals(Object.getPrototypeOf(data.value), Object.getPrototypeOf(initialBuffer), "Cloned buffers have the same type");
            assert_not_equals(data.value, initialBuffer, "Buffer of cloned response stream is a clone of the original buffer");
        });
    }, "Check response clone use structureClone for teed ReadableStreams (" + bufferType  + "chunk)");
}

var arrayBuffer = new ArrayBuffer(16);
testReadableStreamClone(new Int8Array(arrayBuffer, 1), "Int8Array");
testReadableStreamClone(new Int16Array(arrayBuffer, 2, 2), "Int16Array");
testReadableStreamClone(new Int32Array(arrayBuffer), "Int32Array");
testReadableStreamClone(arrayBuffer, "ArrayBuffer");
testReadableStreamClone(new Uint8Array(arrayBuffer), "Uint8Array");
testReadableStreamClone(new Uint8ClampedArray(arrayBuffer), "Uint8ClampedArray");
testReadableStreamClone(new Uint16Array(arrayBuffer, 2), "Uint16Array");
testReadableStreamClone(new Uint32Array(arrayBuffer), "Uint32Array");
testReadableStreamClone(new Float32Array(arrayBuffer), "Float32Array");
testReadableStreamClone(new Float64Array(arrayBuffer), "Float64Array");
testReadableStreamClone(new DataView(arrayBuffer, 2, 8), "DataView");
