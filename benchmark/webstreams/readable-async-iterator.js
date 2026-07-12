'use strict';
const common = require('../common.js');
const {
  ReadableStream,
} = require('node:stream/web');

const bench = common.createBenchmark(main, {
  n: [1e5],
  type: ['normal', 'bytes'],
});


async function main({ n, type }) {
  const rs = type === 'bytes' ?
    new ReadableStream({
      type: 'bytes',
      pull: function(controller) {
        controller.enqueue(new Uint8Array(1));
      },
    }) :
    new ReadableStream({
      pull: function(controller) {
        controller.enqueue(1);
      },
    });

  let x = 0;

  bench.start();
  if (type === 'bytes') {
    for await (const chunk of rs) {
      x += chunk.byteLength;
      if (x > n) {
        break;
      }
    }
  } else {
    for await (const chunk of rs) {
      x += chunk;
      if (x > n) {
        break;
      }
    }
  }
  // Use x to ensure V8 does not optimize away the loop as a noop.
  console.assert(x);
  bench.end(n);
}
