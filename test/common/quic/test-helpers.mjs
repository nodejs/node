// start demo data
// taken from @fails-components/webtransport tests
// by the original author
function createBytesChunk(length) {
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
  const length = arrays.reduce((acc, curr) => acc + curr.length, 0);
  const result = new Uint8Array(length);
  let pos = 0;
  let array = 0;
  while (pos < length) {
    const curArr = arrays[array];
    const curLen = curArr.byteLength;
    const dest = new Uint8Array(result.buffer, result.byteOffset + pos, curLen);
    dest.set(curArr);
    array++;
    pos += curArr.byteLength;
  }
}
