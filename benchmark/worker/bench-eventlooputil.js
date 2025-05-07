'use strict';

const common = require('../common.js');
const { Worker, parentPort } = require('worker_threads');

if (process.argv[2] === 'idle cats') {
  return parentPort.once('message', () => {});
}

const bench = common.createBenchmark(main, {
  n: [1e7],
  method: [
    'ELU_simple',
    'ELU_passed',
  ],
});

function main({ method, n }) {
  switch (method) {
    case 'ELU_simple':
      benchELUSimple(n);
      break;
    case 'ELU_passed':
      benchELUPassed(n);
      break;
    default:
      throw new Error(`Unsupported method ${method}`);
  }
}

function benchELUSimple(n) {
  const worker = new Worker(__filename, { argv: ['idle cats'] });

  spinUntilIdle(worker, () => {
    bench.start();
    for (let i = 0; i < n; i++)
      worker.performance.eventLoopUtilization();
    bench.end(n);
    worker.postMessage('bye');
  });
}

function benchELUPassed(n) {
  const worker = new Worker(__filename, { argv: ['idle cats'] });

  spinUntilIdle(worker, () => {
    let elu = worker.performance.eventLoopUtilization();
    bench.start();
    for (let i = 0; i < n; i++)
      elu = worker.performance.eventLoopUtilization(elu);
    bench.end(n);
    worker.postMessage('bye');
  });
}

function spinUntilIdle(w, cb) {
  const t = w.performance.eventLoopUtilization();
  if (t.idle + t.active > 0)
    return process.nextTick(cb);
  setTimeout(() => spinUntilIdle(w, cb), 1);
}
