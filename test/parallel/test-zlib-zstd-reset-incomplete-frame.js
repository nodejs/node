'use strict';

// Tests that reset() refuses to run only when an incomplete zstd frame has
// already emitted output.
//
// ZSTD_reset_session_only cancels unflushed internal data, so write()-then-
// reset() with no emitted bytes is safe. Once flush() (or a write that filled
// the output buffer) has written a fragment out, resetting would append the
// next frame to that fragment and produce an undecodable stream.

require('../common');
const assert = require('assert');
const { finished } = require('stream/promises');
const test = require('node:test');
const zlib = require('zlib');

test('ZstdCompress reset throws when an incomplete frame has emitted output',
     async () => {
       const stream = zlib.createZstdCompress();
       const chunks = [];
       stream.on('data', (chunk) => chunks.push(chunk));

       stream.write(Buffer.from('hello'));
       await new Promise((resolve) => stream.flush(resolve));
       assert.ok(Buffer.concat(chunks).length > 0);

       // A fragment of the frame is already outside the compressor, so reset()
       // must refuse instead of silently producing a stream that cannot be
       // decoded.
       stream.reset();
       stream.end(Buffer.from('world'));

       await assert.rejects(finished(stream), {
         code: 'ERR_ZLIB_INCOMPLETE_FRAME',
       });
     });

test('ZstdCompress reset throws when write itself emitted frame output',
     async () => {
       const stream = zlib.createZstdCompress();
       const chunks = [];
       stream.on('data', (chunk) => chunks.push(chunk));

       // Fill a buffer that is large enough for zstd to emit compressed bytes
       // during write(), without an explicit flush().
       const input = Buffer.allocUnsafe(512 * 1024);
       for (let i = 0; i < input.length; i++) {
         input[i] = i & 0xff;
       }
       await new Promise((resolve, reject) => {
         stream.write(input, (err) => {
           if (err) {
             reject(err);
           } else {
             resolve();
           }
         });
       });
       assert.ok(Buffer.concat(chunks).length > 0);

       stream.reset();
       stream.end(Buffer.from('world'));

       await assert.rejects(finished(stream), {
         code: 'ERR_ZLIB_INCOMPLETE_FRAME',
       });
     });

test('ZstdCompress reset after write without emitted output still works',
     async () => {
       const stream = zlib.createZstdCompress();
       const chunks = [];
       stream.on('data', (chunk) => chunks.push(chunk));

       // Small writes often stay buffered inside zstd until flush/end.
       await new Promise((resolve, reject) => {
         stream.write(Buffer.from('hello'), (err) => {
           if (err) {
             reject(err);
           } else {
             resolve();
           }
         });
       });
       assert.strictEqual(Buffer.concat(chunks).length, 0);

       // No bytes have left the compressor, so session reset is allowed.
       stream.reset();
       stream.end(Buffer.from('world'));
       await finished(stream);

       assert.strictEqual(
         zlib.zstdDecompressSync(Buffer.concat(chunks)).toString(),
         'world',
       );
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
