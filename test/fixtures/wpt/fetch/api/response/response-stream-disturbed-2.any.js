// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream
// META: script=./response-stream-disturbed-util.js

async function createResponseWithLockedReadableStream(bodySource, callback) {
  const response = await responseFromBodySource(bodySource);
    response.body.getReader();
    return callback(response);
}

for (const bodySource of ["fetch", "stream", "string"]) {
    promise_test(function(test) {
        return createResponseWithLockedReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.blob());
        });
    }, `Getting blob after getting a locked Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithLockedReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.text());
        });
    }, `Getting text after getting a locked Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithLockedReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.json());
        });
    }, `Getting json after getting a locked Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithLockedReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.arrayBuffer());
        });
    }, `Getting arrayBuffer after getting a locked Response body (body source: ${bodySource})`);
}
