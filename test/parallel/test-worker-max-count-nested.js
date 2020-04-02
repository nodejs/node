'use strict';
const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');
const { Worker } = require('worker_threads');

if (process.argv[2] === 'child') {

  // Checks that warning is emitted in the main thread,
  // even tho the offending Worker is created from within
  // another worker.
  common.expectWarning({
    Warning: [
      'Too many active worker threads (2). Performance may be degraded. ' +
      '(--max-worker-threads=1)'
    ] });

  const expr = `
    setInterval(() => {}, 10);
    const { Worker, parentPort } = require('worker_threads');
    const expr = 'setInterval(() => {}, 10);';
    const worker = new Worker(expr, { eval: true });
    worker.on('online', () => {
      worker.terminate();
      parentPort.postMessage({});
    });
  `;

  const worker = new Worker(expr, { eval: true });
  worker.on('message', () => worker.terminate());

} else {
  const child = spawn(
    process.execPath,
    ['--max-worker-threads=1', __filename, 'child'],
    { stdio: 'inherit' });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
}
