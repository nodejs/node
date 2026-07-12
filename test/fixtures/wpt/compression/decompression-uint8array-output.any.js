// META: global=window,worker,shadowrealm
// META: script=resources/decompression-input.js

'use strict';

for (const [format, chunkValue] of compressedBytes) {
  promise_test(async t => {
    const ds = new DecompressionStream(format);
    const reader = ds.readable.getReader();
    const writer = ds.writable.getWriter();
    const writePromise = writer.write(chunkValue);
    const { value } = await reader.read();
    assert_equals(value.constructor, Uint8Array, "type should match");
    await writePromise;
  }, `decompressing ${format} output should give Uint8Array chunks`);
}
