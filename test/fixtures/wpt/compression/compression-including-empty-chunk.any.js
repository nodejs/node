// META: global=window,worker,shadowrealm
// META: script=third_party/pako/pako_inflate.min.js
// META: script=resources/decompress.js
// META: script=resources/formats.js
// META: timeout=long

'use strict';

// This test asserts that compressing '' doesn't affect the compressed data.
// Example: compressing ['Hello', '', 'Hello'] results in 'HelloHello'

async function compressChunkList(chunkList, format) {
  const cs = new CompressionStream(format);
  const writer = cs.writable.getWriter();
  for (const chunk of chunkList) {
    const chunkByte = new TextEncoder().encode(chunk);
    writer.write(chunkByte);
  }
  const closePromise = writer.close();
  const out = [];
  const reader = cs.readable.getReader();
  let totalSize = 0;
  while (true) {
    const { value, done } = await reader.read();
    if (done)
      break;
    out.push(value);
    totalSize += value.byteLength;
  }
  await closePromise;
  const concatenated = new Uint8Array(totalSize);
  let offset = 0;
  for (const array of out) {
    concatenated.set(array, offset);
    offset += array.byteLength;
  }
  return concatenated;
}

const chunkLists = [
  ['', 'Hello', 'Hello'],
  ['Hello', '', 'Hello'],
  ['Hello', 'Hello', '']
];
const expectedValue = new TextEncoder().encode('HelloHello');

for (const format of formats) {
  for (const chunkList of chunkLists) {
    promise_test(async t => {
      const compressedData = await compressChunkList(chunkList, format);
      const decompressedData = await decompressDataOrPako(compressedData, format);
      // check that we got the same result as our original string
      assert_array_equals(expectedValue, decompressedData, 'value should match');
    }, `the result of compressing [${chunkList}] with ${format} should be 'HelloHello'`);
  }
}
