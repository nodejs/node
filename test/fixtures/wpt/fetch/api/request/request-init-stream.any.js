// META: global=window,worker

"use strict";

const duplex = "half";
const method = "POST";

test(() => {
  const body = new ReadableStream();
  const request = new Request("...", { method, body, duplex });
  assert_equals(request.body, body);
}, "Constructing a Request with a stream holds the original object.");

test((t) => {
  const body = new ReadableStream();
  body.getReader();
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body, duplex }));
}, "Constructing a Request with a stream on which getReader() is called");

test((t) => {
  const body = new ReadableStream();
  body.getReader().read();
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body, duplex }));
}, "Constructing a Request with a stream on which read() is called");

promise_test(async (t) => {
  const body = new ReadableStream({ pull: c => c.enqueue(new Uint8Array()) });
  const reader = body.getReader();
  await reader.read();
  reader.releaseLock();
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body, duplex }));
}, "Constructing a Request with a stream on which read() and releaseLock() are called");

test((t) => {
  const request = new Request("...", { method: "POST", body: "..." });
  request.body.getReader();
  assert_throws_js(TypeError, () => new Request(request));
  // This doesn't throw.
  new Request(request, { body: "..." });
}, "Constructing a Request with a Request on which body.getReader() is called");

test((t) => {
  const request = new Request("...", { method: "POST", body: "..." });
  request.body.getReader().read();
  assert_throws_js(TypeError, () => new Request(request));
  // This doesn't throw.
  new Request(request, { body: "..." });
}, "Constructing a Request with a Request on which body.getReader().read() is called");

promise_test(async (t) => {
  const request = new Request("...", { method: "POST", body: "..." });
  const reader = request.body.getReader();
  await reader.read();
  reader.releaseLock();
  assert_throws_js(TypeError, () => new Request(request));
  // This doesn't throw.
  new Request(request, { body: "..." });
}, "Constructing a Request with a Request on which read() and releaseLock() are called");

test((t) => {
  new Request("...", { method, body: null });
}, "It is OK to omit .duplex when the body is null.");

test((t) => {
  new Request("...", { method, body: "..." });
}, "It is OK to omit .duplex when the body is a string.");

test((t) => {
  new Request("...", { method, body: new Uint8Array(3) });
}, "It is OK to omit .duplex when the body is a Uint8Array.");

test((t) => {
  new Request("...", { method, body: new Blob([]) });
}, "It is OK to omit .duplex when the body is a Blob.");

test((t) => {
  const body = new ReadableStream();
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body }));
}, "It is error to omit .duplex when the body is a ReadableStream.");

test((t) => {
  new Request("...", { method, body: null, duplex: "half" });
}, "It is OK to set .duplex = 'half' when the body is null.");

test((t) => {
  new Request("...", { method, body: "...", duplex: "half" });
}, "It is OK to set .duplex = 'half' when the body is a string.");

test((t) => {
  new Request("...", { method, body: new Uint8Array(3), duplex: "half" });
}, "It is OK to set .duplex = 'half' when the body is a Uint8Array.");

test((t) => {
  new Request("...", { method, body: new Blob([]), duplex: "half" });
}, "It is OK to set .duplex = 'half' when the body is a Blob.");

test((t) => {
  const body = new ReadableStream();
  new Request("...", { method, body, duplex: "half" });
}, "It is OK to set .duplex = 'half' when the body is a ReadableStream.");

test((t) => {
  const body = null;
  const duplex = "full";
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body, duplex }));
}, "It is error to set .duplex = 'full' when the body is null.");

test((t) => {
  const body = "...";
  const duplex = "full";
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body, duplex }));
}, "It is error to set .duplex = 'full' when the body is a string.");

test((t) => {
  const body = new Uint8Array(3);
  const duplex = "full";
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body, duplex }));
}, "It is error to set .duplex = 'full' when the body is a Uint8Array.");

test((t) => {
  const body = new Blob([]);
  const duplex = "full";
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body, duplex }));
}, "It is error to set .duplex = 'full' when the body is a Blob.");

test((t) => {
  const body = new ReadableStream();
  const duplex = "full";
  assert_throws_js(TypeError,
                   () => new Request("...", { method, body, duplex }));
}, "It is error to set .duplex = 'full' when the body is a ReadableStream.");

test((t) => {
  const body = new ReadableStream();
  const duplex = "half";
  const req1 = new Request("...", { method, body, duplex });
  const req2 = new Request(req1);
}, "It is OK to omit duplex when init.body is not given and input.body is given.");

