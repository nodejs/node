// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream

function createResponseWithDisturbedReadableStream(callback) {
    return fetch("../resources/data.json").then(function(response) {
        var reader = response.body.getReader();
        reader.read();
        return callback(response);
    });
}

promise_test(function(test) {
    return createResponseWithDisturbedReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.blob());
    });
}, "Getting blob after reading the Response body");

promise_test(function(test) {
    return createResponseWithDisturbedReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.text());
    });
}, "Getting text after reading the Response body");

promise_test(function(test) {
    return createResponseWithDisturbedReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.json());
    });
}, "Getting json after reading the Response body");

promise_test(function(test) {
    return createResponseWithDisturbedReadableStream(function(response) {
        return promise_rejects_js(test, TypeError, response.arrayBuffer());
    });
}, "Getting arrayBuffer after reading the Response body");
