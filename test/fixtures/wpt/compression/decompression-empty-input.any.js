// META: global=window,worker,shadowrealm

'use strict';

const emptyValues = [
  ["gzip", new Uint8Array([31, 139, 8, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0])],
  ["deflate", new Uint8Array([120, 156, 3, 0, 0, 0, 0, 1])],
  ["deflate-raw", new Uint8Array([1, 0, 0, 255, 255])],
  ["brotli", new Uint8Array([0xa1, 0x01])],
];

for (const [format, emptyValue] of emptyValues) {
  promise_test(async t => {
    const ds = new DecompressionStream(format);
    const reader = ds.readable.getReader();
    const writer = ds.writable.getWriter();
    const writePromise = writer.write(emptyValue);
    writer.close();
    const { value, done } = await reader.read();
    assert_true(done, "read() should set done");
    assert_equals(value, undefined, "value should be undefined");
    await writePromise;
  }, `decompressing ${format} empty input should work`);
}
