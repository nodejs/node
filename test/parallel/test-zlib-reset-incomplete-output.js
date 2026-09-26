'use strict';

// Tests that reset() refuses to run on gzip once an incomplete member has
// already emitted output.
//
// deflateReset() is equivalent to deflateEnd + deflateInit. gzip writes a
// header on the first write, so those bytes cannot be taken back.
//
// zlib-wrapped deflate still allows reset after flush: official
// test-zlib-dictionary.js discards the first member and reuses the
// compressor. Raw deflate has no wrapper header: a small write may emit
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

test('Gzip reset throws when write has emitted wrapper output', async () => {
  const stream = zlib.createGzip();
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

test('Gzip reset throws when flush has emitted incomplete output', async () => {
  const stream = zlib.createGzip();
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

test('Gzip flush followed by end still produces a valid stream', async () => {
  const stream = zlib.createGzip();
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));

  stream.write(Buffer.from('hello'));
  await new Promise((resolve) => stream.flush(resolve));
  stream.end(Buffer.from('world'));
  await finished(stream);

  assert.strictEqual(
    zlib.gunzipSync(Buffer.concat(chunks)).toString(),
    'helloworld',
  );
});

test('Gzip reset before any write still works', async () => {
  const stream = zlib.createGzip();
  const chunks = [];
  stream.on('data', (chunk) => chunks.push(chunk));

  stream.reset();
  stream.end(Buffer.from('hello'));
  await finished(stream);

  assert.strictEqual(
    zlib.gunzipSync(Buffer.concat(chunks)).toString(),
    'hello',
  );
});

test('Deflate reset after flush still works when first output is discarded',
     async () => {
       const stream = zlib.createDeflate();
       const chunks = [];
       let take = false;
       stream.on('data', (chunk) => {
         if (take) {
           chunks.push(chunk);
         }
       });

       stream.write(Buffer.from('hello'));
       await new Promise((resolve) => stream.flush(resolve));
       stream.reset();
       take = true;
       stream.end(Buffer.from('world'));
       await finished(stream);

       assert.strictEqual(
         zlib.inflateSync(Buffer.concat(chunks)).toString(),
         'world',
       );
     });

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
