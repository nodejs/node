'use strict';

const common = require('../common');
const Readable = require('stream').Readable;

const bench = common.createBenchmark(main, {
  n: [1e5],
  sync: ['yes', 'no'],
});

async function main({ n, sync }) {
  sync = sync === 'yes';

  const s = new Readable({
    objectMode: true,
    read() {
      if (sync) {
        this.push(1);
      } else {
        process.nextTick(() => {
          this.push(1);
        });
      }
    },
  });

  bench.start();

  let x = 0;
  for await (const chunk of s) {
    x += chunk;
    if (x > n) {
      break;
    }
  }

  // Use x to ensure V8 does not optimize away the loop as a noop.
  console.assert(x);

  bench.end(n);
}
