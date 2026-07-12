// META: global=window,worker,shadowrealm
// META: script=third_party/pako/pako_inflate.min.js
// META: script=resources/concatenate-stream.js
// META: script=resources/decompress.js
// META: script=resources/formats.js
// META: timeout=long

'use strict';

// This test verifies that a large flush output will not truncate the
// final results.

async function compressData(chunk, format) {
  const cs = new CompressionStream(format);
  const writer = cs.writable.getWriter();
  writer.write(chunk);
  writer.close();
  return await concatenateStream(cs.readable);
}

// JSON-encoded array of 10 thousands numbers ("[0,1,2,...]"). This produces 48_891 bytes of data.
const fullData = new TextEncoder().encode(JSON.stringify(Array.from({ length: 10_000 }, (_, i) => i)));
const data = fullData.subarray(0, 35_579);
const expectedValue = data;

for (const format of formats) {
  promise_test(async t => {
    const compressedData = await compressData(data, format);
    const decompressedData = await decompressDataOrPako(compressedData, format);
    // check that we got the same result as our original string
    assert_array_equals(decompressedData, expectedValue, 'value should match');
  }, `${format} compression with large flush output`);
}
