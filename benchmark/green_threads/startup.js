'use strict';
const common = require('../common.js');
const { once } = require('events');

const bench = common.createBenchmark(main, {
  kind: ['green-thread', 'green-thread-warm', 'worker', 'vm-context'],
  n: [50],
}, {
  flags: ['--experimental-green-threads', '--no-warnings'],
});

// Starts `n` of `kind` at once, and measures until each has sent a message.
async function main({ kind, n }) {
  switch (kind) {
    case 'green-thread':
    case 'green-thread-warm': {
      const { GreenThread, setWarmPoolSize } = require('node:green_threads');
      if (kind === 'green-thread-warm') await setWarmPoolSize(n);
      bench.start();
      const greenThreads = Array.from(
        { length: n }, () => new GreenThread('postMessage(0);'));
      await Promise.all(greenThreads.map((g) => once(g, 'message')));
      bench.end(n);
      await setWarmPoolSize(0);
      await Promise.all(greenThreads.map((g) => g.terminate()));
      break;
    }
    case 'worker': {
      const { Worker } = require('worker_threads');
      bench.start();
      const workers = Array.from({ length: n }, () => new Worker(
        "require('worker_threads').parentPort.postMessage(0);",
        { eval: true }));
      await Promise.all(workers.map((w) => once(w, 'message')));
      bench.end(n);
      await Promise.all(workers.map((w) => w.terminate()));
      break;
    }
    case 'vm-context': {
      const vm = require('vm');
      bench.start();
      for (let i = 0; i < n; i++) {
        vm.runInContext('0', vm.createContext());
      }
      bench.end(n);
      break;
    }
  }
}
