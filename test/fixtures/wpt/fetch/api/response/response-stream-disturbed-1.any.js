// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream

function createResponseWithReadableStream(callback) {
    return fetch("../resources/data.json").then(function(response) {
        var reader = response.body.getReader();
        reader.releaseLock();
        return callback(response);
    });
}

promise_test(function() {
    return createResponseWithReadableStream(function(response) {
        return response.blob().then(function(blob) {
            assert_true(blob instanceof Blob);
        });
    });
}, "Getting blob after getting the Response body - not disturbed, not locked");

promise_test(function() {
    return createResponseWithReadableStream(function(response) {
        return response.text().then(function(text) {
            assert_true(text.length > 0);
        });
    });
}, "Getting text after getting the Response body - not disturbed, not locked");

promise_test(function() {
    return createResponseWithReadableStream(function(response) {
        return response.json().then(function(json) {
            assert_equals(typeof json, "object");
        });
    });
}, "Getting json after getting the Response body - not disturbed, not locked");

promise_test(function() {
    return createResponseWithReadableStream(function(response) {
        return response.arrayBuffer().then(function(arrayBuffer) {
            assert_true(arrayBuffer.byteLength > 0);
        });
    });
}, "Getting arrayBuffer after getting the Response body - not disturbed, not locked");
