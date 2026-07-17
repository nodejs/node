// META: global=window,worker,shadowrealm
// META: script=third_party/pako/pako_inflate.min.js
// META: script=resources/decompress.js
// META: script=resources/formats.js
// META: timeout=long

'use strict';

const SMALL_FILE = "/media/foo.vtt";
const LARGE_FILE = "/media/test-av-384k-44100Hz-1ch-320x240-30fps-10kfr.webm";

let dataPromiseList = [
  ["empty data", Promise.resolve(new Uint8Array(0))],
  ["small amount data", fetch(SMALL_FILE).then(response => response.bytes())],
  ["large amount data", fetch(LARGE_FILE).then(response => response.bytes())],
];

async function compressArrayBuffer(input, format) {
  const cs = new CompressionStream(format);
  const writer = cs.writable.getWriter();
  writer.write(input);
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

test(() => {
  assert_throws_js(TypeError, () => {
    const transformer = new CompressionStream("nonvalid");
  }, "non supported format should throw");
}, "CompressionStream constructor should throw on invalid format");

for (const format of formats) {
  for (const [label, dataPromise] of dataPromiseList) {
    promise_test(async () => {
      const bufferView = await dataPromise;
      const compressedData = await compressArrayBuffer(bufferView, format);
      const decompressedData = await decompressDataOrPako(compressedData, format);
      // check that we got the same result as our original string
      assert_array_equals(decompressedData, bufferView, 'value should match');
    }, `${format} ${label} should be reinflated back to its origin`);
  }
}
