'use strict';

const common = require('../common.js');
const { Worker } = require('worker_threads');

const bench = common.createBenchmark(main, {
  n: [1, 4, 8, 16],
});

function main({ n }) {
  const workers = [];
  let online = 0;

  bench.start();
  for (let i = 0; i < n; i++) {
    // Keep workers alive so their teardown does not compete with startup.
    const worker = new Worker('setInterval(() => {}, 1e6)', { eval: true });
    workers.push(worker);
    worker.on('online', () => {
      if (++online === n) {
        bench.end(n);
        for (const worker of workers) {
          worker.terminate();
        }
      }
    });
  }
}
