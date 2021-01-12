// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream

function createResponseWithLockedReadableStream(callback) {
    return fetch("../resources/data.json").then(function(response) {
        var reader = response.body.getReader();
        return callback(response);
    });
}

promise_test(function(test) {
    return createResponseWithLockedReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.blob());
    });
}, "Getting blob after getting a locked Response body");

promise_test(function(test) {
    return createResponseWithLockedReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.text());
    });
}, "Getting text after getting a locked Response body");

promise_test(function(test) {
    return createResponseWithLockedReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.json());
    });
}, "Getting json after getting a locked Response body");

promise_test(function(test) {
    return createResponseWithLockedReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.arrayBuffer());
    });
}, "Getting arrayBuffer after getting a locked Response body");
