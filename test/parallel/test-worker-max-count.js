'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');
const { Worker } = require('worker_threads');
const { cpus } = require('os');

if (process.argv[2] === 'child') {
  const cpu_count = cpus().length;

  common.expectWarning({
    Warning: [
      'Too many active worker threads (2). Performance may be degraded. ' +
      '(--max-worker-threads=1)'
    ] });

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

  doTest().then(doTest).then(common.mustCall());

} else {
  const child = spawn(
    process.execPath,
    ['--max-worker-threads=1', __filename, 'child'],
    { stdio: 'inherit' });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
