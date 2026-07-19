// META: global=window,worker,shadowrealm

'use strict';

const compressedBytes = [
  ["deflate", [120, 156, 75, 52, 48, 52, 50, 54, 49, 53, 3, 0, 8, 136, 1, 199]],
  ["gzip", [31, 139, 8, 0, 0, 0, 0, 0, 0, 3, 75, 52, 48, 52, 2, 0, 216, 252, 63, 136, 4, 0, 0, 0]],
  ["deflate-raw", [
    0x00, 0x06, 0x00, 0xf9, 0xff, 0x41, 0x42, 0x43,
    0x44, 0x45, 0x46, 0x01, 0x00, 0x00, 0xff, 0xff,
  ]],
  ["brotli", [0x21, 0x08, 0x00, 0x04, 0x66, 0x6F, 0x6F, 0x03]]
];
// These chunk values below were chosen to make the length of the compressed
// output be a multiple of 8 bytes.
const expectedChunkValue = new Map(Object.entries({
  "deflate": new TextEncoder().encode('a0123456'),
  "gzip": new TextEncoder().encode('a012'),
  "deflate-raw": new TextEncoder().encode('ABCDEF'),
  "brotli": new TextEncoder().encode('foo'),
}));

const bufferSourceChunks = compressedBytes.map(([format, bytes]) => [format, [
  {
    name: 'ArrayBuffer',
    value: new Uint8Array(bytes).buffer
  },
  {
    name: 'Int8Array',
    value: new Int8Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Uint8Array',
    value: new Uint8Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Uint8ClampedArray',
    value: new Uint8ClampedArray(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Int16Array',
    value: new Int16Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Uint16Array',
    value: new Uint16Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Int32Array',
    value: new Int32Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Uint32Array',
    value: new Uint32Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Float16Array',
    value: () => new Float16Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Float32Array',
    value: new Float32Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'Float64Array',
    value: new Float64Array(new Uint8Array(bytes).buffer)
  },
  {
    name: 'DataView',
    value: new DataView(new Uint8Array(bytes).buffer)
  },
]]);

for (const [format, chunks] of bufferSourceChunks) {
  for (const chunk of chunks) {
    promise_test(async t => {
      const ds = new DecompressionStream(format);
      const reader = ds.readable.getReader();
      const writer = ds.writable.getWriter();
      const writePromise = writer.write(typeof chunk.value === 'function' ? chunk.value() : chunk.value);
      writer.close();
      const { value } = await reader.read();
      assert_array_equals(Array.from(value), expectedChunkValue.get(format), 'value should match');
    }, `chunk of type ${chunk.name} should work for ${format}`);
  }
}
