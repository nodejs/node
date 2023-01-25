'use strict';
const common = require('../common.js');
const {
  ReadableStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [1e5],
});


async function main({ n }) {
  const rs = new ReadableStream({
    pull: function(controller) {
      controller.enqueue(1);
    },
  });

  let x = 0;

  bench.start();
  for await (const chunk of rs) {
    x += chunk;
    if (x > n) {
      break;
    }
  }
  // Use x to ensure V8 does not optimize away the loop as a noop.
  console.assert(x);
  bench.end(n);
}
