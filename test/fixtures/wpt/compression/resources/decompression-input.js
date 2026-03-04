const deflateChunkValue = new Uint8Array([120, 156, 75, 173, 40, 72, 77, 46, 73, 77, 81, 200, 47, 45, 41, 40, 45, 1, 0, 48, 173, 6, 36]);
const gzipChunkValue = new Uint8Array([31, 139, 8, 0, 0, 0, 0, 0, 0, 3, 75, 173, 40, 72, 77, 46, 73, 77, 81, 200, 47, 45, 41, 40, 45, 1, 0, 176, 1, 57, 179, 15, 0, 0, 0]);
const deflateRawChunkValue = new Uint8Array([
    0x4b, 0xad, 0x28, 0x48, 0x4d, 0x2e, 0x49, 0x4d, 0x51, 0xc8,
    0x2f, 0x2d, 0x29, 0x28, 0x2d, 0x01, 0x00,
]);
const brotliChunkValue = new Uint8Array([
  0x21, 0x38, 0x00, 0x04, 0x65, 0x78, 0x70, 0x65,
  0x63, 0x74, 0x65, 0x64, 0x20, 0x6F, 0x75, 0x74,
  0x70, 0x75, 0x74, 0x03
]);

const compressedBytes = [
  ["deflate", deflateChunkValue],
  ["gzip", gzipChunkValue],
  ["deflate-raw", deflateRawChunkValue],
  ["brotli", brotliChunkValue],
];

const expectedChunkValue = new TextEncoder().encode('expected output');
