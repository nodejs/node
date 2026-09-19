'use strict';

// Tests that reset() refuses to run while a zstd frame is still in progress.
//
// A frame is only complete once ZSTD_compressStream2() has been called with
// ZSTD_e_end and returned 0. Resetting earlier dropped the frame state, and any
// bytes already written out (for example by flush()) stayed in the output
// stream as an unusable fragment. The next frame was then appended to that
// fragment, so the resulting stream could not be decompressed.

require('../common');
const assert = require('assert');
const { finished } = require('stream/promises');
const test = require('node:test');
const zlib = require('zlib');

test('ZstdCompress reset throws when a frame is incomplete', async () => {
  const stream = zlib.createZstdCompress();
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));

  stream.write(Buffer.from('hello'));
  await new Promise((resolve) => stream.flush(resolve));

  // The frame started by write() is still open, so reset() must refuse
  // instead of silently producing a stream that cannot be decoded.
  stream.reset();
  stream.end(Buffer.from('world'));

  await assert.rejects(finished(stream), {
    code: 'ERR_ZLIB_INCOMPLETE_FRAME',
  });
});

test('ZstdCompress flush followed by end still produces a valid stream',
     async () => {
       const stream = zlib.createZstdCompress();
       const chunks = [];
       stream.on('data', (chunk) => chunks.push(chunk));

       stream.write(Buffer.from('hello'));
       await new Promise((resolve) => stream.flush(resolve));
       stream.end(Buffer.from('world'));
       await finished(stream);

       assert.strictEqual(
         zlib.zstdDecompressSync(Buffer.concat(chunks)).toString(),
         'helloworld',
       );
     });

test('ZstdCompress reset before any write still works', async () => {
  const stream = zlib.createZstdCompress();
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));

  // No frame has been started yet, so reset() is allowed.
  stream.reset();
  stream.end(Buffer.from('hello'));
  await finished(stream);

  assert.strictEqual(
    zlib.zstdDecompressSync(Buffer.concat(chunks)).toString(),
    'hello',
  );
});
