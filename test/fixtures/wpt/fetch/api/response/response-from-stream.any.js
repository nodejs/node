// META: global=window,worker

"use strict";

test(() => {
  const stream = new ReadableStream();
  stream.getReader();
  assert_throws_js(TypeError, () => new Response(stream));
}, "Constructing a Response with a stream on which getReader() is called");

test(() => {
  const stream = new ReadableStream();
  stream.getReader().read();
  assert_throws_js(TypeError, () => new Response(stream));
}, "Constructing a Response with a stream on which read() is called");

promise_test(async () => {
  const stream = new ReadableStream({ pull: c => c.enqueue(new Uint8Array()) }),
        reader = stream.getReader();
  await reader.read();
  reader.releaseLock();
  assert_throws_js(TypeError, () => new Response(stream));
}, "Constructing a Response with a stream on which read() and releaseLock() are called");
