// Flags: --max-worker-threads=0
'use strict';
const common = require('../common');
const { Worker } = require('worker_threads');
const { cpus } = require('os');

const cpu_count = cpus().length;

// This may need to be updated in the future if there are
// any other warnings expected. For now, no warnings are
// emitted with this code.
process.on('warning', common.mustNotCall());

const expr = 'setInterval(() => {}, 10);';

function makeWorker(workers) {
  return new Promise((res) => {
    const worker = new Worker(expr, { eval: true });
    worker.on('online', res);
    workers.push(worker);
  });
}

async function doTest() {
  const workers = [];
  const list = [];
  for (let n = 0; n < cpu_count; n++)
    list.push(makeWorker(workers));
  await Promise.all(list);
  workers.forEach((i) => i.terminate());
}

doTest().then(common.mustCall());
