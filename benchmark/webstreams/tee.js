'use strict';
const common = require('../common.js');
const { ReadableStream } = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [1e5],
  type: ['normal', 'bytes'],
});

async function main({ n, type }) {
  let i = 0;
  const source = type === 'bytes' ?
    {
      type: 'bytes',
      pull(controller) {
        if (i++ < n) controller.enqueue(new Uint8Array(16));
        else controller.close();
      },
    } :
    {
      pull(controller) {
        if (i++ < n) controller.enqueue('a');
        else controller.close();
      },
    };

  const rs = new ReadableStream(source);
  const [branch1, branch2] = rs.tee();
  const reader1 = branch1.getReader();
  const reader2 = branch2.getReader();
  let reads = 0;

  bench.start();
  for (;;) {
    const [result1, result2] = await Promise.all([
      reader1.read(),
      reader2.read(),
    ]);
    if (result1.done || result2.done) break;
    reads++;
  }
  bench.end(reads);
  console.assert(reads === n);
}
