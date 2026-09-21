'use strict';

// Tests that reset() refuses to run on gzip / zlib-wrapped deflate once an
// incomplete member has already emitted output.
//
// deflateReset() is equivalent to deflateEnd + deflateInit. Bytes that have
// already been written out cannot be taken back, so the next member would be
// appended to that fragment and gunzip/inflate fail with Z_DATA_ERROR.
//
// gzip and zlib deflate emit a header on the first write, so write-then-reset
// is already unsafe. Raw deflate has no wrapper header: a small write may emit
// nothing, and flush+reset still concatenates.

require('../common');
const assert = require('assert');
const { finished } = require('stream/promises');
const test = require('node:test');
const zlib = require('zlib');

async function writeHello(stream) {
  await new Promise((resolve, reject) => {
    stream.write(Buffer.from('hello'), (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
}

for (const [name, create, decompress] of [
  ['Gzip', zlib.createGzip, zlib.gunzipSync],
  ['Deflate', zlib.createDeflate, zlib.inflateSync],
]) {
  test(`${name} reset throws when write has emitted wrapper output`,
       async () => {
         const stream = create();
         const chunks = [];
         stream.on('data', (chunk) => chunks.push(chunk));

         await writeHello(stream);
         assert.ok(Buffer.concat(chunks).length > 0);

         stream.reset();
         stream.end(Buffer.from('world'));

         await assert.rejects(finished(stream), {
           code: 'ERR_ZLIB_INCOMPLETE_FRAME',
         });
       });

  test(`${name} reset throws when flush has emitted incomplete output`,
       async () => {
         const stream = create();
         const chunks = [];
         stream.on('data', (chunk) => chunks.push(chunk));

         stream.write(Buffer.from('hello'));
         await new Promise((resolve) => stream.flush(resolve));
         assert.ok(Buffer.concat(chunks).length > 0);

         stream.reset();
         stream.end(Buffer.from('world'));

         await assert.rejects(finished(stream), {
           code: 'ERR_ZLIB_INCOMPLETE_FRAME',
         });
       });

  test(`${name} flush followed by end still produces a valid stream`,
       async () => {
         const stream = create();
         const chunks = [];
         stream.on('data', (chunk) => chunks.push(chunk));

         stream.write(Buffer.from('hello'));
         await new Promise((resolve) => stream.flush(resolve));
         stream.end(Buffer.from('world'));
         await finished(stream);

         assert.strictEqual(
           decompress(Buffer.concat(chunks)).toString(),
           'helloworld',
         );
       });

  test(`${name} reset before any write still works`, async () => {
    const stream = create();
    const chunks = [];
    stream.on('data', (chunk) => chunks.push(chunk));

    stream.reset();
    stream.end(Buffer.from('hello'));
    await finished(stream);

    assert.strictEqual(
      decompress(Buffer.concat(chunks)).toString(),
      'hello',
    );
  });
}

test('DeflateRaw reset after write without emitted output still works',
     async () => {
       const stream = zlib.createDeflateRaw();
       const chunks = [];
       stream.on('data', (chunk) => chunks.push(chunk));

       await writeHello(stream);
       assert.strictEqual(Buffer.concat(chunks).length, 0);

       stream.reset();
       stream.end(Buffer.from('world'));
       await finished(stream);

       assert.strictEqual(
         zlib.inflateRawSync(Buffer.concat(chunks)).toString(),
         'world',
       );
     });

test('DeflateRaw flush followed by reset still concatenates', async () => {
  const stream = zlib.createDeflateRaw();
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));

  stream.write(Buffer.from('hello'));
  await new Promise((resolve) => stream.flush(resolve));
  stream.reset();
  stream.end(Buffer.from('world'));
  await finished(stream);

  assert.strictEqual(
    zlib.inflateRawSync(Buffer.concat(chunks)).toString(),
    'helloworld',
  );
});
