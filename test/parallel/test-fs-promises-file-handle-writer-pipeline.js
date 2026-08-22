// Flags: --experimental-stream-iter
'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const { open } = fs.promises;
const path = require('path');
const tmpdir = require('../common/tmpdir');
const { pipeTo, text } = require('stream/iter');
const { compressGzip, decompressGzip } = require('zlib/iter');

tmpdir.refresh();

const tmpDir = tmpdir.path;

// =============================================================================
// Round-trip: pull -> compress -> writer, pull -> decompress -> verify
// =============================================================================

async function testCompressRoundTrip() {
  const srcPath = path.join(tmpDir, 'writer-rt-src.txt');
  const gzPath = path.join(tmpDir, 'writer-rt.gz');
  const original = 'Round trip compression test data. '.repeat(2000);
  fs.writeFileSync(srcPath, original);

  // Compress: pull -> gzip -> writer
  {
    const rfh = await open(srcPath, 'r');
    const wfh = await open(gzPath, 'w');
    const w = wfh.writer({ autoClose: true });
    await pipeTo(rfh.pull(), compressGzip(), w);
    await rfh.close();
  }

  // Verify compressed file is smaller
  const compressedSize = fs.statSync(gzPath).size;
  assert.ok(
    compressedSize < Buffer.byteLength(original),
    `Compressed ${compressedSize} should be < original ${Buffer.byteLength(original)}`,
  );

  // Decompress: pull -> gunzip -> text -> verify
  {
    const rfh = await open(gzPath, 'r');
    const result = await text(rfh.pull(decompressGzip()));
    await rfh.close();
    assert.strictEqual(result, original);
  }
}

// =============================================================================
// Large file write - write 1MB in 64KB chunks
// =============================================================================

async function testLargeFileWrite() {
  const filePath = path.join(tmpDir, 'writer-large.bin');
  const fh = await open(filePath, 'w');
  const w = fh.writer();

  const chunkSize = 65536;
  const totalSize = 1024 * 1024; // 1MB
  const chunk = Buffer.alloc(chunkSize, 0x42);
  let written = 0;

  while (written < totalSize) {
    await w.write(chunk);
    written += chunkSize;
  }

  const totalBytes = await w.end();
  await fh.close();

  assert.strictEqual(totalBytes, totalSize);
  assert.strictEqual(fs.statSync(filePath).size, totalSize);

  // Verify content
  const data = fs.readFileSync(filePath);
  for (let i = 0; i < data.length; i++) {
    if (data[i] !== 0x42) {
      assert.fail(`Byte at offset ${i} is ${data[i]}, expected 0x42`);
    }
  }
}

Promise.all([
  testCompressRoundTrip(),
  testLargeFileWrite(),
]).then(common.mustCall());
