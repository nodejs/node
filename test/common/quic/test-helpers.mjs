import assert from 'node:assert';
// start demo data
// taken from @fails-components/webtransport tests
// by the original author
export function createBytesChunk(length) {
  assert.strictEqual(length % 4, 0);
  const workArray = new Array(length / 2);
  for (let i = 0; i < length / 4; i++) {
    workArray[2 * i + 1] = length % 0xffff;
    workArray[2 * i] = i;
  }
  const helper = new Uint16Array(workArray);
  const toreturn = new Uint8Array(
    helper.buffer,
    helper.byteOffset,
    helper.byteLength,
  );
  return toreturn;
}

// The number in the comments, help you identify the chunk, as it is the length first two bytes
// this is helpful, when debugging buffer passing
export const KNOWN_BYTES_LONG = [
  createBytesChunk(60000), // 96, 234
  createBytesChunk(12), // 0, 12
  createBytesChunk(50000), // 195, 80
  createBytesChunk(1600).buffer, // 6, 64 we use buffer here to increae test coverage
  createBytesChunk(20000), // 78, 32
  createBytesChunk(30000), // 117, 48
];

// end demo data

export function uint8concat(arrays) {
  const length = arrays.reduce((acc, curr) => acc + curr.byteLength, 0);
  const result = new Uint8Array(length);
  let pos = 0;
  let array = 0;
  while (pos < length) {
    const curArr = arrays[array];
    const curLen = curArr.byteLength;
    const dest = new Uint8Array(result.buffer, result.byteOffset + pos, curLen);
    dest.set(new Uint8Array(curArr));
    array++;
    pos += curArr.byteLength;
  }
  return result;
}

export function equalUint8Arrays(arr1, arr2) {
  assert.ok(arr1.byteLength === arr2.byteLength,
            'Array content differs in length ' +
            arr1.byteLength +
            ' vs. ' +
            arr2.byteLength +
            ' diff: ' +
            (arr1.byteLength - arr2.byteLength));
  for (let i = 0; i < arr1.byteLength; i++) {
    assert.ok(arr1[i] === arr2[i],
              'Array content differs at ' +
              i +
              ' arr1: ' +
              arr1[i] +
              ' arr2: ' +
              arr2[i]);
  }
}
