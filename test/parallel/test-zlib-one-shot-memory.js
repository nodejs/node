'use strict';
const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');

// Test that one-shot convenience methods (async and sync) do not retain
// oversized unpooled backing ArrayBuffers (such as the default chunkSize)
// when output size is small.

const smallPayload = Buffer.from('hello world'.repeat(10));
const chunkSize = 128 * 1024;

// Test async and sync compress convenience methods
for (const method of [
  'gzip',
  'deflate',
  'deflateRaw',
  'brotliCompress',
  'zstdCompress',
]) {
  zlib[method](smallPayload, { chunkSize }, common.mustSucceed((buf) => {
    assert.ok(Buffer.isBuffer(buf));

    // The backing ArrayBuffer should not retain the 128KB unpooled chunk
    assert.ok(
      buf.buffer.byteLength < chunkSize,
      `${method} result backing ArrayBuffer should not retain chunkSize padding`
    );
    assert.ok(
      buf.buffer.byteLength <= Buffer.poolSize,
      `${method} result backing ArrayBuffer should not exceed Buffer.poolSize`
    );
  }));

  // Test sync convenience method
  const syncMethod = `${method}Sync`;
  const syncResult = zlib[syncMethod](smallPayload, { chunkSize });
  assert.ok(Buffer.isBuffer(syncResult));
  assert.ok(
    syncResult.buffer.byteLength < chunkSize,
    `${syncMethod} result backing ArrayBuffer should not retain chunkSize padding`
  );
  assert.ok(
    syncResult.buffer.byteLength <= Buffer.poolSize,
    `${syncMethod} result backing ArrayBuffer should not exceed Buffer.poolSize`
  );
}

// Test decompress convenience methods
zlib.gzip(smallPayload, common.mustSucceed((compressed) => {
  zlib.gunzip(compressed, { chunkSize }, common.mustSucceed((decompressed) => {
    assert.strictEqual(decompressed.toString(), smallPayload.toString());
    assert.ok(
      decompressed.buffer.byteLength < chunkSize,
      'gunzip result backing ArrayBuffer should not retain chunkSize padding'
    );
    assert.ok(
      decompressed.buffer.byteLength <= Buffer.poolSize,
      'gunzip result backing ArrayBuffer should not exceed Buffer.poolSize'
    );
  }));

  const syncDecompressed = zlib.gunzipSync(compressed, { chunkSize });
  assert.strictEqual(syncDecompressed.toString(), smallPayload.toString());
  assert.ok(
    syncDecompressed.buffer.byteLength < chunkSize,
    'gunzipSync result backing ArrayBuffer should not retain chunkSize padding'
  );
  assert.ok(
    syncDecompressed.buffer.byteLength <= Buffer.poolSize,
    'gunzipSync result backing ArrayBuffer should not exceed Buffer.poolSize'
  );
}));

// Test error path on invalid data
{
  const invalidGzip = Buffer.from([0x1f, 0x8b, 0x00, 0x00]);
  zlib.gunzip(invalidGzip, { chunkSize }, common.mustCall((err) => {
    assert.ok(err);
  }));
}
