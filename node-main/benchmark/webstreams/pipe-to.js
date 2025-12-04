'use strict';
const common = require('../common.js');
const {
  ReadableStream,
  WritableStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [5e5],
  highWaterMarkR: [512, 1024, 2048, 4096],
  highWaterMarkW: [512, 1024, 2048, 4096],
});


async function main({ n, highWaterMarkR, highWaterMarkW }) {
  const b = Buffer.alloc(1024);
  let i = 0;
  const rs = new ReadableStream({
    highWaterMark: highWaterMarkR,
    pull: function(controller) {
      if (i++ < n) {
        controller.enqueue(b);
      } else {
        controller.close();
      }
    },
  });
  const ws = new WritableStream({
    highWaterMark: highWaterMarkW,
    write(chunk, controller) {},
    close() { bench.end(n); },
  });

  bench.start();
  rs.pipeTo(ws);
}
