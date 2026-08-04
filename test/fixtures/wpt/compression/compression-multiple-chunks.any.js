// META: global=window,worker,shadowrealm
// META: script=third_party/pako/pako_inflate.min.js
// META: script=resources/decompress.js
// META: script=resources/formats.js
// META: timeout=long

'use strict';

// This test asserts that compressing multiple chunks should work.

// Example: ('Hello', 3) => TextEncoder().encode('HelloHelloHello')
function makeExpectedChunk(input, numberOfChunks) {
  const expectedChunk = input.repeat(numberOfChunks);
  return new TextEncoder().encode(expectedChunk);
}

// Example: ('Hello', 3, 'deflate') => compress ['Hello', 'Hello', Hello']
async function compressMultipleChunks(input, numberOfChunks, format) {
  const cs = new CompressionStream(format);
  const writer = cs.writable.getWriter();
  const chunk = new TextEncoder().encode(input);
  for (let i = 0; i < numberOfChunks; ++i) {
    writer.write(chunk);
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

const hello = 'Hello';

for (const format of formats) {
  for (let numberOfChunks = 2; numberOfChunks <= 16; ++numberOfChunks) {
    promise_test(async t => {
      const compressedData = await compressMultipleChunks(hello, numberOfChunks, format);
      const decompressedData = await decompressDataOrPako(compressedData, format);
      const expectedValue = makeExpectedChunk(hello, numberOfChunks);
      // check that we got the same result as our original string
      assert_array_equals(decompressedData, expectedValue, 'value should match');
    }, `compressing ${numberOfChunks} chunks with ${format} should work`);
  }
}
