'use strict';
require('../common');
const assert = require('assert');
const test = require('node:test');
const zlib = require('zlib');
const { DecompressionStream } = require('stream/web');

async function assertDecompressionStreamRejects(format, chunks) {
  await assert.rejects(
    Array.fromAsync(
      new Blob(chunks).stream().pipeThrough(new DecompressionStream(format))
    ),
    { name: 'TypeError' },
  );
}

test('DecompressionStream deflate emits TypeError on trailing data', async () => {
  const valid = new Uint8Array([120, 156, 75, 4, 0, 0, 98, 0, 98]); // deflate('a')
  const empty = new Uint8Array(1);
  const invalid = new Uint8Array([...valid, ...empty]);
  const double = new Uint8Array([...valid, ...valid]);

  for (const chunks of [[invalid], [valid, empty], [valid, valid], [double]]) {
    await assertDecompressionStreamRejects('deflate', chunks);
  }
});

test('DecompressionStream gzip emits TypeError on trailing data', async () => {
  const valid = new Uint8Array([31, 139, 8, 0, 0, 0, 0, 0, 0, 19, 75, 4,
                                0, 67, 190, 183, 232, 1, 0, 0, 0]); // gzip('a')
  const empty = new Uint8Array(1);
  const invalid = new Uint8Array([...valid, ...empty]);
  const double = new Uint8Array([...valid, ...valid]);
  for (const chunks of [[invalid], [valid, empty], [valid, valid], [double]]) {
    await assertDecompressionStreamRejects('gzip', chunks);
  }
});

test('DecompressionStream brotli emits TypeError on trailing data', async () => {
  const valid = zlib.brotliCompressSync(Buffer.from('a'));
  const empty = new Uint8Array(1);
  const invalid = new Uint8Array([...valid, ...empty]);
  const double = new Uint8Array([...valid, ...valid]);
  for (const chunks of [[invalid], [valid, empty], [valid, valid], [double]]) {
    await assertDecompressionStreamRejects('brotli', chunks);
  }
});

test('zlib sync decompression honors rejectGarbageAfterEnd', () => {
  const valid = new Uint8Array([31, 139, 8, 0, 0, 0, 0, 0, 0, 19, 75, 4,
                                0, 67, 190, 183, 232, 1, 0, 0, 0]); // gzip('a')
  const double = new Uint8Array([...valid, ...valid]);

  assert.deepStrictEqual(zlib.gunzipSync(double), Buffer.from('aa'));
  assert.throws(
    () => zlib.gunzipSync(double, { rejectGarbageAfterEnd: true }),
    { code: 'ERR_TRAILING_JUNK_AFTER_STREAM_END', name: 'TypeError' },
  );

  const brotli = zlib.brotliCompressSync(Buffer.from('a'));
  const brotliDouble = Buffer.concat([brotli, brotli]);

  assert.deepStrictEqual(zlib.brotliDecompressSync(brotliDouble), Buffer.from('a'));
  assert.throws(
    () => zlib.brotliDecompressSync(brotliDouble, { rejectGarbageAfterEnd: true }),
    { code: 'ERR_TRAILING_JUNK_AFTER_STREAM_END', name: 'TypeError' },
  );
});
