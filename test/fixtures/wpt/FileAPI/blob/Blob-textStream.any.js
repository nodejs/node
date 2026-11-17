// META: title=Blob textStream
// META: global=window,worker
// META: script=../support/Blob.js
'use strict';

async function readAllChunks(stream) {
  const reader = stream.getReader();
  const chunks = [];
  while (true) {
    const {done, value} = await reader.read();
    if (done) {
      break;
    }
    chunks.push(value);
  }
  return chunks;
}

test(() => {
  assert_true('textStream' in Blob.prototype, "textStream exists on Blob.prototype");
  assert_equals(typeof Blob.prototype.textStream, "function", "Blob.prototype.textStream is a function");
}, "textStream method existence");

promise_test(async () => {
  const blob = new Blob(["hello world"]);
  const stream = blob.textStream();
  assert_true(stream instanceof ReadableStream, "textStream() returns a ReadableStream");

  const chunks = await readAllChunks(stream);
  assert_greater_than(chunks.length, 0);
  for (const chunk of chunks) {
    assert_equals(typeof chunk, "string", "each chunk should be a string");
  }
  assert_equals(chunks.join(""), "hello world", "concatenated chunks match the blob content");
}, "Blob.textStream() basic functionality");

promise_test(async () => {
  const blob = new Blob();
  const stream = blob.textStream();
  assert_true(stream instanceof ReadableStream);
  const chunks = await readAllChunks(stream);
  assert_equals(chunks.length, 0, "no chunks should be read from empty blob textStream");
}, "Blob.textStream() with empty blob");

promise_test(async () => {
  const blob = new Blob(["hello ", "world"]);
  const stream = blob.textStream();
  const chunks = await readAllChunks(stream);
  assert_equals(chunks.join(""), "hello world");
}, "Blob.textStream() with multi-part blob");

promise_test(async () => {
  const buffer = new Uint8Array([0x68, 0x00, 0x65, 0x00, 0x6c, 0x00, 0x6c, 0x00, 0x6f, 0x00]); // "hello" in UTF-16LE
  const blob = new Blob([buffer], {
    type: "text/plain; charset=utf-16le"
  });
  const stream = blob.textStream();
  const chunks = await readAllChunks(stream);
  assert_equals(chunks.join(""), "h\0e\0l\0l\0o\0", "ignores charset=utf-16le type parameter");
}, "Blob.textStream() ignores type charset (UTF-16LE)");

promise_test(async () => {
  const blob = new Blob([new TextEncoder().encode("hello")], {
    type: "text/plain; charset=invalid-charset"
  });
  const stream = blob.textStream();
  const chunks = await readAllChunks(stream);
  assert_equals(chunks.join(""), "hello", "ignores invalid-charset type parameter and decodes as UTF-8");
}, "Blob.textStream() ignores invalid type charset (invalid-charset)");

promise_test(async () => {
  const blob = new Blob([new TextEncoder().encode("é")], {
    type: "text/plain; charset=iso-8859-1"
  });
  const stream = blob.textStream();
  const chunks = await readAllChunks(stream);
  assert_equals(chunks.join(""), "é", "ignores charset=iso-8859-1 type parameter and decodes as UTF-8");
}, "Blob.textStream() ignores type charset (iso-8859-1)");

promise_test(async () => {
  const blob = new Blob(["hello"]);
  const stream1 = blob.textStream();
  const stream2 = blob.textStream();
  assert_not_equals(stream1, stream2, "multiple calls return new streams");

  const chunks1 = await readAllChunks(stream1);
  const chunks2 = await readAllChunks(stream2);
  assert_equals(chunks1.join(""), "hello");
  assert_equals(chunks2.join(""), "hello");
}, "Blob.textStream() can be called multiple times");
