'use strict';
const common = require('../common.js');
const {
  ReadableStream,
  TextEncoderStream,
  TextDecoderStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [1e5],
  kind: ['encode', 'decode'],
  len: [16, 1024],
});

async function main({ n, kind, len }) {
  const encoded = new TextEncoder().encode('a'.repeat(len));
  const decoded = 'a'.repeat(len);
  let i = 0;
  const rs = new ReadableStream({
    pull(controller) {
      if (i++ < n) {
        controller.enqueue(kind === 'encode' ? decoded : encoded);
      } else {
        controller.close();
      }
    },
  });
  const ts = kind === 'encode' ?
    new TextEncoderStream() :
    new TextDecoderStream();

  const reader = rs.pipeThrough(ts).getReader();
  bench.start();
  for (;;) {
    const { done } = await reader.read();
    if (done) break;
  }
  bench.end(n);
}
