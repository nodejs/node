// META: global=window,worker,shadowrealm
// META: script=resources/decompression-input.js

'use strict';

for (const [format, chunk] of compressedBytes) {
  promise_test(async t => {
    const ds = new DecompressionStream(format);
    const reader = ds.readable.getReader();
    const writer = ds.writable.getWriter();
    const writePromise = writer.write(chunk);
    const { done, value } = await reader.read();
    assert_array_equals(Array.from(value), expectedChunkValue, "value should match");
  }, `decompressing ${format} input should work`);
}
