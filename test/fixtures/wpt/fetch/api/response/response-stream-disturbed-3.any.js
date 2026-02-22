// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream
// META: script=./response-stream-disturbed-util.js

async function createResponseWithDisturbedReadableStream(bodySource, callback) {
  const response = await responseFromBodySource(bodySource);
    const reader = response.body.getReader();
    reader.read();
    return callback(response);
}

for (const bodySource of ["fetch", "stream", "string"]) {
    promise_test(function(test) {
        return createResponseWithDisturbedReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.blob());
        });
    }, `Getting blob after reading the Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithDisturbedReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.text());
        });
    }, `Getting text after reading the Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithDisturbedReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.json());
        });
    }, `Getting json after reading the Response body (body source: ${bodySource})`);

    promise_test(function(test) {
        return createResponseWithDisturbedReadableStream(bodySource, function(response) {
            return promise_rejects_js(test, TypeError, response.arrayBuffer());
        });
    }, `Getting arrayBuffer after reading the Response body (body source: ${bodySource})`);
}
