'use strict';
const common = require('../common.js');
const {
  ReadableStream,
  TransformStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [5e5],
  kind: ['default', 'transform'],
});

async function main({ n, kind }) {
  const b = Buffer.alloc(64);
  let i = 0;
  const rs = new ReadableStream({
    pull(controller) {
      if (i++ < n) {
        controller.enqueue(b);
      } else {
        controller.close();
      }
    },
  });
  const ts = kind === 'default' ?
    new TransformStream() :
    new TransformStream({
      transform(chunk, controller) { controller.enqueue(chunk); },
    });

  const reader = rs.pipeThrough(ts).getReader();
  bench.start();
  for (;;) {
    const { done } = await reader.read();
    if (done) break;
  }
  bench.end(n);
}
