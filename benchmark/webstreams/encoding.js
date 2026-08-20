'use strict';
const common = require('../common.js');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  chunkSize: [4096],
  totalBytes: [64 << 20],
  kind: ['encode', 'decode', 'transcode'],
});

async function main({ chunkSize, totalBytes, kind }) {
  const line = 'time=2024-01-01T00:00:00.000Z level=info request=42 ' +
    'method=GET path=/api/v1/items status=200 duration=13ms\n';
  const byteChunk = Buffer.alloc(chunkSize, line);
  const stringChunk = byteChunk.toString();
  const nChunks = Math.ceil(totalBytes / chunkSize);

  let i = 0;
  const input = kind === 'encode' ? stringChunk : byteChunk;
  const source = new ReadableStream({
    pull(controller) {
      if (i++ < nChunks) {
        controller.enqueue(input);
      } else {
        controller.close();
      }
    },
  });

  let stream = source;
  switch (kind) {
    case 'encode':
      stream = source.pipeThrough(new TextEncoderStream());
      break;
    case 'decode':
      stream = source.pipeThrough(new TextDecoderStream());
      break;
    case 'transcode':
      stream = source
        .pipeThrough(new TextDecoderStream())
        .pipeThrough(new TextEncoderStream());
      break;
  }

  let processed = 0;
  bench.start();
  for await (const chunk of stream) {
    processed += chunk.length;
  }
  bench.end(totalBytes / (1024 * 1024));
  assert.strictEqual(processed, nChunks * chunkSize);
}
