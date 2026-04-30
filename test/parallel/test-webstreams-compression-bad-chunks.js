'use strict';
require('../common');
const assert = require('assert');
const test = require('node:test');
const { CompressionStream, DecompressionStream } = require('stream/web');

// Verify that writing invalid (non-BufferSource) chunks to
// CompressionStream and DecompressionStream properly rejects
// on both the write and the read side, instead of hanging.

const badChunks = [
  { name: 'undefined', value: undefined, code: 'ERR_INVALID_ARG_TYPE' },
  { name: 'null', value: null, code: 'ERR_STREAM_NULL_VALUES' },
  { name: 'number', value: 3.14, code: 'ERR_INVALID_ARG_TYPE' },
  { name: 'object', value: {}, code: 'ERR_INVALID_ARG_TYPE' },
  { name: 'array', value: [65], code: 'ERR_INVALID_ARG_TYPE' },
  {
    name: 'SharedArrayBuffer',
    value: new SharedArrayBuffer(1),
    code: 'ERR_INVALID_ARG_TYPE',
  },
  {
    name: 'Uint8Array backed by SharedArrayBuffer',
    value: new Uint8Array(new SharedArrayBuffer(1)),
    code: 'ERR_INVALID_ARG_TYPE',
  },
];

for (const format of ['deflate', 'deflate-raw', 'gzip', 'brotli']) {
  for (const { name, value, code } of badChunks) {
    const expected = { name: 'TypeError', code };

    test(`CompressionStream rejects bad chunk (${name}) for ${format}`, async () => {
      const cs = new CompressionStream(format);
      const writer = cs.writable.getWriter();
      const reader = cs.readable.getReader();

      const writePromise = writer.write(value);
      const readPromise = reader.read();

      await assert.rejects(writePromise, expected);
      await assert.rejects(readPromise, expected);
    });

    test(`DecompressionStream rejects bad chunk (${name}) for ${format}`, async () => {
      const ds = new DecompressionStream(format);
      const writer = ds.writable.getWriter();
      const reader = ds.readable.getReader();

      const writePromise = writer.write(value);
      const readPromise = reader.read();

      await assert.rejects(writePromise, expected);
      await assert.rejects(readPromise, expected);
    });
  }
}

// Verify that decompression errors (e.g. corrupt data) are surfaced as
// TypeError, not plain Error, per the Compression Streams spec.
for (const format of ['deflate', 'deflate-raw', 'gzip', 'brotli']) {
  test(`DecompressionStream surfaces corrupt data as TypeError for ${format}`, async () => {
    const ds = new DecompressionStream(format);
    const writer = ds.writable.getWriter();
    const reader = ds.readable.getReader();

    const corruptData = new Uint8Array([0, 1, 2, 3, 4, 5]);

    writer.write(corruptData).catch(() => {});
    reader.read().catch(() => {});

    await assert.rejects(writer.close(), { name: 'TypeError' });
    await assert.rejects(reader.closed, { name: 'TypeError' });
  });
}
