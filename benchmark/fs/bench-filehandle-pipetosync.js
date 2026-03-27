// Benchmark: pipeToSync with sync compression transforms.
// Measures fully synchronous file-to-file pipeline (no threadpool, no promises).
'use strict';

const common = require('../common.js');
const fs = require('fs');
const { openSync, closeSync, writeSync, unlinkSync } = fs;

const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();
const srcFile = tmpdir.resolve(`.removeme-sync-bench-src-${process.pid}`);
const dstFile = tmpdir.resolve(`.removeme-sync-bench-dst-${process.pid}`);

const bench = common.createBenchmark(main, {
  compression: ['gzip', 'deflate', 'brotli', 'zstd'],
  filesize: [1024 * 1024, 16 * 1024 * 1024, 64 * 1024 * 1024],
  n: [5],
}, {
  flags: ['--experimental-stream-iter'],
});

function main({ compression, filesize, n }) {
  // Create the fixture file with repeating lowercase ASCII
  const chunk = Buffer.alloc(Math.min(filesize, 64 * 1024), 'abcdefghij');
  const fd = openSync(srcFile, 'w');
  let remaining = filesize;
  while (remaining > 0) {
    const toWrite = Math.min(remaining, chunk.length);
    writeSync(fd, chunk, 0, toWrite);
    remaining -= toWrite;
  }
  closeSync(fd);

  const { pipeToSync } = require('stream/iter');
  const {
    compressGzipSync,
    compressDeflateSync,
    compressBrotliSync,
    compressZstdSync,
  } = require('zlib/iter');
  const { open } = fs.promises;

  const compressFactory = {
    gzip: compressGzipSync,
    deflate: compressDeflateSync,
    brotli: compressBrotliSync,
    zstd: compressZstdSync,
  }[compression];

  // Stateless uppercase transform (sync)
  const upper = (chunks) => {
    if (chunks === null) return null;
    const out = new Array(chunks.length);
    for (let j = 0; j < chunks.length; j++) {
      const src = chunks[j];
      const buf = Buffer.allocUnsafe(src.length);
      for (let i = 0; i < src.length; i++) {
        const b = src[i];
        buf[i] = (b >= 0x61 && b <= 0x7a) ? b - 0x20 : b;
      }
      out[j] = buf;
    }
    return out;
  };

  // Use a synchronous wrapper since pipeToSync is fully sync.
  // We need FileHandle for pullSync/writer, so open async then run sync.
  (async () => {
    const srcFh = await open(srcFile, 'r');
    const dstFh = await open(dstFile, 'w');

    // Warm up
    runSync(srcFh, dstFh, upper, compressFactory, pipeToSync);

    // Reset file positions for the benchmark
    await srcFh.close();
    await dstFh.close();

    bench.start();
    let totalBytes = 0;
    for (let i = 0; i < n; i++) {
      const src = await open(srcFile, 'r');
      const dst = await open(dstFile, 'w');
      totalBytes += runSync(src, dst, upper, compressFactory, pipeToSync);
      await src.close();
      await dst.close();
    }
    bench.end(totalBytes / (1024 * 1024));

    cleanup();
  })();
}

function runSync(srcFh, dstFh, upper, compressFactory, pipeToSync) {
  const w = dstFh.writer();
  pipeToSync(srcFh.pullSync(upper, compressFactory()), w);
  return w.endSync();
}

function cleanup() {
  try { unlinkSync(srcFile); } catch { /* Ignore */ }
  try { unlinkSync(dstFile); } catch { /* Ignore */ }
}
