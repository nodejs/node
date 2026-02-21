'use strict';
const common = require('../common');
const assert = require('assert');
const { DecompressionStream, CompressionStream } = require('stream/web');

// Minimal gzip-compressed bytes for "hello"
const compressedGzip = new Uint8Array([
  31, 139, 8, 0, 0, 0, 0, 0, 0, 3,
  203, 72, 205, 201, 201, 7, 0, 134, 166, 16, 54, 5, 0, 0, 0,
]);

async function testDecompressionAcceptsArrayBuffer() {
  const ds = new DecompressionStream('gzip');
  const reader = ds.readable.getReader();
  const writer = ds.writable.getWriter();

  const writePromise = writer.write(compressedGzip.buffer);
  writer.close();

  const chunks = [];
  let done = false;
  while (!done) {
    const { value, done: d } = await reader.read();
    if (value) chunks.push(value);
    done = d;
  }
  await writePromise;
  const out = Buffer.concat(chunks.map((c) => Buffer.from(c)));
  assert.strictEqual(out.toString(), 'hello');
}

async function testCompressionRoundTripWithArrayBuffer() {
  const cs = new CompressionStream('gzip');
  const ds = new DecompressionStream('gzip');

  const csWriter = cs.writable.getWriter();
  const csReader = cs.readable.getReader();
  const dsWriter = ds.writable.getWriter();
  const dsReader = ds.readable.getReader();

  const input = new TextEncoder().encode('hello').buffer;

  await csWriter.write(input);
  csWriter.close();

  await cs.readable.pipeTo(ds.writable);

  const out = [];
  done = false;
  while (!done) {
    const { value, done: d } = await dsReader.read();
    if (value) out.push(value);
    done = d;
  }
  const result = Buffer.concat(out.map((c) => Buffer.from(c)));
  assert.strictEqual(result.toString(), 'hello');
}

Promise.all([
  testDecompressionAcceptsArrayBuffer(),
  testCompressionRoundTripWithArrayBuffer(),
]).then(common.mustCall());
