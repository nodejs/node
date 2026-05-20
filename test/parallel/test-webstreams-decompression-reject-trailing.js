'use strict';
require('../common');
const assert = require('assert');
const test = require('node:test');
const { CompressionStream, DecompressionStream } = require('stream/web');

// Verify that DecompressionStream rejects trailing data after a valid
// compressed payload for all four supported formats (deflate, deflate-raw, gzip, brotli).

const input = Buffer.from('hello');
const trailingJunk = Buffer.from([0xDE, 0xAD]);

async function compress(format, data) {
  const cs = new CompressionStream(format);
  const writer = cs.writable.getWriter();
  writer.write(data);
  writer.close();
  const chunks = await Array.fromAsync(cs.readable);
  return Buffer.concat(chunks.map((c) => Buffer.from(c)));
}

for (const format of ['deflate', 'deflate-raw', 'gzip', 'brotli']) {
  test(`DecompressionStream rejects trailing junk for ${format}`, async () => {
    const compressed = await compress(format, input);
    const withJunk = Buffer.concat([compressed, trailingJunk]);

    const ds = new DecompressionStream(format);
    const writer = ds.writable.getWriter();
    const reader = ds.readable.getReader();

    writer.write(withJunk).catch(() => {});
    writer.close().catch(() => {});

    await assert.rejects(async () => {
      const chunks = [];
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
      }
    }, (err) => {
      assert(err instanceof Error, `Expected Error, got ${err?.constructor?.name}`);
      return true;
    });
  });

  test(`DecompressionStream accepts valid ${format} data without trailing junk`, async () => {
    const compressed = await compress(format, input);

    const ds = new DecompressionStream(format);
    const writer = ds.writable.getWriter();

    writer.write(compressed);
    writer.close();

    const chunks = await Array.fromAsync(ds.readable);
    const result = Buffer.concat(chunks.map((c) => Buffer.from(c)));
    assert.strictEqual(result.toString(), 'hello');
  });
}

// Extra: Verify that trailing data is also rejected when passed as a separate
// chunk after the valid compressed data has been fully written.
for (const format of ['deflate', 'deflate-raw', 'gzip', 'brotli']) {
  test(`DecompressionStream rejects trailing junk as separate chunk for ${format}`, async () => {
    const compressed = await compress(format, input);

    const ds = new DecompressionStream(format);
    const writer = ds.writable.getWriter();
    const reader = ds.readable.getReader();

    writer.write(compressed).catch(() => {});
    writer.write(trailingJunk).catch(() => {});
    writer.close().catch(() => {});

    await assert.rejects(async () => {
      while (true) {
        const { done } = await reader.read();
        if (done) break;
      }
    }, (err) => {
      assert(err instanceof Error, `Expected Error, got ${err?.constructor?.name}`);
      return true;
    });
  });
}
