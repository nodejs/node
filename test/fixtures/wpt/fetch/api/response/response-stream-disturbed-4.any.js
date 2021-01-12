// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream

function createResponseWithCancelledReadableStream(callback) {
    return fetch("../resources/data.json").then(function(response) {
        response.body.cancel();
        return callback(response);
    });
}

promise_test(function(test) {
    return createResponseWithCancelledReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.blob());
    });
}, "Getting blob after cancelling the Response body");

promise_test(function(test) {
    return createResponseWithCancelledReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.text());
    });
}, "Getting text after cancelling the Response body");

promise_test(function(test) {
    return createResponseWithCancelledReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.json());
    });
}, "Getting json after cancelling the Response body");

promise_test(function(test) {
    return createResponseWithCancelledReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.arrayBuffer());
    });
}, "Getting arrayBuffer after cancelling the Response body");
