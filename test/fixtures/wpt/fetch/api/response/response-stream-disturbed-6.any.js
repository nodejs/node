// META: global=window,worker
// META: title=ReadableStream disturbed tests, via Response's bodyUsed property

"use strict";

test(() => {
  const stream = new ReadableStream();
  const response = new Response(stream);
  assert_false(response.bodyUsed, "On construction");

  const reader = stream.getReader();
  assert_false(response.bodyUsed, "After getting a reader");

  reader.read();
  assert_true(response.bodyUsed, "After calling stream.read()");
}, "A non-closed stream on which read() has been called");

test(() => {
  const stream = new ReadableStream();
  const response = new Response(stream);
  assert_false(response.bodyUsed, "On construction");

  const reader = stream.getReader();
  assert_false(response.bodyUsed, "After getting a reader");

  reader.cancel();
  assert_true(response.bodyUsed, "After calling stream.cancel()");
}, "A non-closed stream on which cancel() has been called");

test(() => {
  const stream = new ReadableStream({
    start(c) {
      c.close();
    }
  });
  const response = new Response(stream);
  assert_false(response.bodyUsed, "On construction");

  const reader = stream.getReader();
  assert_false(response.bodyUsed, "After getting a reader");

  reader.read();
  assert_true(response.bodyUsed, "After calling stream.read()");
}, "A closed stream on which read() has been called");

test(() => {
  const stream = new ReadableStream({
    start(c) {
      c.error(new Error("some error"));
    }
  });
  const response = new Response(stream);
  assert_false(response.bodyUsed, "On construction");

  const reader = stream.getReader();
  assert_false(response.bodyUsed, "After getting a reader");

  reader.read().then(() => { }, () => { });
  assert_true(response.bodyUsed, "After calling stream.read()");
}, "An errored stream on which read() has been called");

test(() => {
  const stream = new ReadableStream({
    start(c) {
      c.error(new Error("some error"));
    }
  });
  const response = new Response(stream);
  assert_false(response.bodyUsed, "On construction");

  const reader = stream.getReader();
  assert_false(response.bodyUsed, "After getting a reader");

  reader.cancel().then(() => { }, () => { });
  assert_true(response.bodyUsed, "After calling stream.cancel()");
}, "An errored stream on which cancel() has been called");
