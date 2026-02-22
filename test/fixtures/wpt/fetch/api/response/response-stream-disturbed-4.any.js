// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream
// META: script=./response-stream-disturbed-util.js

async function createResponseWithCancelledReadableStream(bodySource, callback) {
  const response = await responseFromBodySource(bodySource);
    response.body.cancel();
    return callback(response);
}

for (const bodySource of ["fetch", "stream", "string"]) {
    promise_test(function(test) {
        return createResponseWithCancelledReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.blob());
        });
    }, `Getting blob after cancelling the Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithCancelledReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.text());
        });
    }, `Getting text after cancelling the Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithCancelledReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.json());
        });
    }, `Getting json after cancelling the Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithCancelledReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.arrayBuffer());
        });
    }, `Getting arrayBuffer after cancelling the Response body (body source: ${bodySource})`);
}
