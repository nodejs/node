'use strict';
const common = require('../common.js');
const { gzipSync, deflateSync } = require('node:zlib');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  chunkSize: [4096, 65536],
  totalBytes: [64 << 20],
  kind: ['compress', 'decompress'],
  format: ['gzip', 'deflate'],
});

// Repetitive but non-trivial payload.
function makePayload(totalBytes, chunkSize) {
  const line = 'time=2024-01-01T00:00:00.000Z level=info request=42 ' +
    'method=GET path=/api/v1/items status=200 duration=13ms\n';
  const chunk = Buffer.alloc(chunkSize, line);
  const chunks = [];
  for (let n = 0; n < totalBytes; n += chunk.length)
    chunks.push(chunk);
  return chunks;
}

function makeSource(chunks) {
  let i = 0;
  return new ReadableStream({
    pull(controller) {
      if (i < chunks.length) {
        controller.enqueue(chunks[i++]);
      } else {
        controller.close();
      }
    },
  });
}

async function main({ chunkSize, totalBytes, kind, format }) {
  let chunks = makePayload(totalBytes, chunkSize);

  let stream;
  if (kind === 'compress') {
    stream = new CompressionStream(format);
  } else {
    // Rechunk the compressed payload so the decompressor sees the
    // configured chunk size.
    const compress = format === 'gzip' ? gzipSync : deflateSync;
    const compressed = compress(Buffer.concat(chunks));
    chunks = [];
    for (let off = 0; off < compressed.length; off += chunkSize)
      chunks.push(compressed.subarray(off, off + chunkSize));
    stream = new DecompressionStream(format);
  }

  const source = makeSource(chunks);

  let outputBytes = 0;
  bench.start();
  for await (const chunk of source.pipeThrough(stream)) {
    outputBytes += chunk.byteLength;
  }
  bench.end(totalBytes / (1024 * 1024));

  if (kind === 'decompress')
    assert.strictEqual(outputBytes, totalBytes);
  else
    assert.notStrictEqual(outputBytes, 0);
}
