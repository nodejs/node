'use strict';
const common = require('../common.js');
const {
  ReadableStream,
  WritableStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [5e5],
  highWaterMarkR: [1, 1024, 4096],
  highWaterMarkW: [1, 1024, 4096],
});


async function main({ n, highWaterMarkR, highWaterMarkW }) {
  const b = Buffer.alloc(1024);
  let i = 0;
  const rs = new ReadableStream({
    pull: function(controller) {
      if (i++ < n) {
        controller.enqueue(b);
      } else {
        controller.close();
      }
    },
  }, { highWaterMark: highWaterMarkR });
  const ws = new WritableStream({
    write(chunk, controller) {},
    close() { bench.end(n); },
  }, { highWaterMark: highWaterMarkW });

  bench.start();
  rs.pipeTo(ws);
}
