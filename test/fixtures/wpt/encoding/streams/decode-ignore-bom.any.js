// META: global=window,worker
// META: script=resources/readable-stream-from-array.js
// META: script=resources/readable-stream-to-array.js

const cases = [
    {encoding: 'utf-8', bytes: [0xEF, 0xBB, 0xBF, 0x61, 0x62, 0x63]},
    {encoding: 'utf-16le', bytes: [0xFF, 0xFE, 0x61, 0x00, 0x62, 0x00, 0x63, 0x00]},
    {encoding: 'utf-16be', bytes: [0xFE, 0xFF, 0x00, 0x61, 0x00, 0x62, 0x00, 0x63]}
];
const BOM = '\uFEFF';

// |inputChunks| is an array of chunks, each represented by an array of
// integers. |ignoreBOM| is true or false. The result value is the output of the
// pipe, concatenated into a single string.
async function pipeAndAssemble(inputChunks, encoding, ignoreBOM) {
  const chunksAsUint8 = inputChunks.map(values => new Uint8Array(values));
  const readable = readableStreamFromArray(chunksAsUint8);
  const outputArray = await readableStreamToArray(readable.pipeThrough(
      new TextDecoderStream(encoding, {ignoreBOM})));
  return outputArray.join('');
}

for (const testCase of cases) {
  for (let splitPoint = 0; splitPoint < 4; ++splitPoint) {
    promise_test(async () => {
      const inputChunks = [testCase.bytes.slice(0, splitPoint),
                           testCase.bytes.slice(splitPoint)];
      const withIgnoreBOM =
            await pipeAndAssemble(inputChunks, testCase.encoding, true);
      assert_equals(withIgnoreBOM, BOM + 'abc', 'BOM should be preserved');

      const withoutIgnoreBOM =
            await pipeAndAssemble(inputChunks, testCase.encoding, false);
      assert_equals(withoutIgnoreBOM, 'abc', 'BOM should be stripped')
    }, `ignoreBOM should work for encoding ${testCase.encoding}, split at ` +
       `character ${splitPoint}`);
  }
}
