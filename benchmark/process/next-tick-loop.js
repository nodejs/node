'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [1e4, 2e4, 4e4],
  loop: [1e2, 2e2],
});

function main({ n, loop }) {
  bench.start();
  run();
  function run() {
    let j = 0;

    function cb() {
      j++;
      if (j === n) {
        loop--;
        if (loop === 0) {
          bench.end(n);
        } else {
          run();
        }
      }
    }

    for (let i = 0; i < n; i++) {
      process.nextTick(cb);
    }
  }
}
