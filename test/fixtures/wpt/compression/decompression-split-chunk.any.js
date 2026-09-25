// META: global=window,worker,shadowrealm
// META: script=resources/decompression-input.js

'use strict';

async function decompressArrayBuffer(input, format, chunkSize) {
  const ds = new DecompressionStream(format);
  const reader = ds.readable.getReader();
  const writer = ds.writable.getWriter();
  for (let beginning = 0; beginning < input.length; beginning += chunkSize) {
    writer.write(input.slice(beginning, beginning + chunkSize));
  }
  writer.close();
  const out = [];
  let totalSize = 0;
  while (true) {
    const { value, done } = await reader.read();
    if (done) break;
    out.push(value);
    totalSize += value.byteLength;
  }
  const concatenated = new Uint8Array(totalSize);
  let offset = 0;
  for (const array of out) {
    concatenated.set(array, offset);
    offset += array.byteLength;
  }
  return concatenated;
}

for (const [format, bytes] of compressedBytes) {
  for (let chunkSize = 1; chunkSize < 16; ++chunkSize) {
    promise_test(async t => {
      const decompressedData = await decompressArrayBuffer(bytes, format, chunkSize);
      assert_array_equals(decompressedData, expectedChunkValue, "value should match");
    }, `decompressing splitted chunk into pieces of size ${chunkSize} should work in ${format}`);
  }
}
