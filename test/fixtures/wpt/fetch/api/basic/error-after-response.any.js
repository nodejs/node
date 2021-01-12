// META: title=Fetch: network timeout after receiving the HTTP response headers
// META: global=window,worker
// META: timeout=long
// META: script=../resources/utils.js

function checkReader(test, reader, promiseToTest)
{
    return reader.read().then((value) => {
        validateBufferFromString(value.value, "TEST_CHUNK", "Should receive first chunk");
        return promise_rejects_js(test, TypeError, promiseToTest(reader));
    });
}

promise_test((test) => {
    return fetch("../resources/bad-chunk-encoding.py?count=1").then((response) => {
        return checkReader(test, response.body.getReader(), reader => reader.read());
    });
}, "Response reader read() promise should reject after a network error happening after resolving fetch promise");

promise_test((test) => {
    return fetch("../resources/bad-chunk-encoding.py?count=1").then((response) => {
        return checkReader(test, response.body.getReader(), reader => reader.closed);
    });
}, "Response reader closed promise should reject after a network error happening after resolving fetch promise");
