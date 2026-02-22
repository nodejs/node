// META: global=window,worker
// META: title=Consuming Response body after getting a ReadableStream
// META: script=./response-stream-disturbed-util.js

for (const bodySource of ["fetch", "stream", "string"]) {
  for (const consumeAs of ["blob", "text", "json", "arrayBuffer"]) {
    promise_test(
      async () => {
        const response = await responseFromBodySource(bodySource);
        response[consumeAs]();
        assert_not_equals(response.body, null);
        assert_throws_js(TypeError, function () {
          response.body.getReader();
        });
      },
      `Getting a body reader after consuming as ${consumeAs} (body source: ${bodySource})`,
    );
  }
}
