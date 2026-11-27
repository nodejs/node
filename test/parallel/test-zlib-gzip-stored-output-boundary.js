'use strict';

// Regression test for https://github.com/nodejs/node/issues/45268.
// With gzip and level 0, deflate_stored() copies input directly into the
// output buffer through the SIMD CRC32 copy routine. An old version of that
// routine stored 16 bytes for the final (len % 16) bytes and wrote past the
// end of the output buffer, corrupting the heap. Exercise output buffers that
// end at every offset modulo 16 and check the round trip.

require('../common');
const assert = require('assert');
const zlib = require('zlib');

for (let i = 0; i < 400; i++) {
  const size = 40000 + i * 517;
  const input = Buffer.alloc(size, i & 0xff);
  for (let j = 0; j < size; j += 97) input[j] = j & 0xff;
  const chunkSize = 65536 + (i % 17);
  const gz = zlib.gzipSync(input, { level: 0, chunkSize });
  assert.deepStrictEqual(zlib.gunzipSync(gz), input);
}

// Small inputs finished straight into a nearly full output buffer.
for (let len = 1; len <= 64; len++) {
  const input = Buffer.alloc(len, 0x61);
  for (let chunkSize = 64; chunkSize <= 96; chunkSize++) {
    const gz = zlib.gzipSync(input, { level: 0, chunkSize });
    assert.deepStrictEqual(zlib.gunzipSync(gz), input);
  }
}
