'use strict';
const common = require('../common');
const assert = require('assert');
const { promisify } = require('util');
const execFile = promisify(require('child_process').execFile);
const { Worker, isMainThread, workerData } = require('worker_threads');

const variant = process.argv[process.argv.length - 1];
switch (true) {
  case variant === 'main-thread': {
    return;
  }
  case variant === 'main-thread-exit': {
    return process.exit(0);
  }
  case variant.startsWith('worker-thread'): {
    const worker = new Worker(__filename, { workerData: variant });
    worker.on('error', common.mustNotCall());
    worker.on('exit', common.mustCall((code) => {
      assert.strictEqual(code, 0);
    }));
    return;
  }
  case !isMainThread: {
    if (workerData === 'worker-thread-exit') {
      process.exit(0);
    }
    return;
  }
}

(async function() {
  for (const { execArgv, variant, warnings } of [
    { execArgv: ['--trace-exit'], variant: 'main-thread-exit', warnings: 1 },
    { execArgv: [], variant: 'main-thread-exit', warnings: 0 },
    { execArgv: ['--trace-exit'], variant: 'main-thread', warnings: 0 },
    { execArgv: [], variant: 'main-thread', warnings: 0 },
    { execArgv: ['--trace-exit'], variant: 'worker-thread-exit', warnings: 1 },
    { execArgv: [], variant: 'worker-thread-exit', warnings: 0 },
    { execArgv: ['--trace-exit'], variant: 'worker-thread', warnings: 0 },
    { execArgv: [], variant: 'worker-thread', warnings: 0 },
  ]) {
    const { stdout, stderr } =
      await execFile(process.execPath, [...execArgv, __filename, variant]);
    assert.strictEqual(stdout, '');
    const actualWarnings =
      stderr.match(/WARNING: Exited the environment with code 0/g);
    if (warnings === 0) {
      assert.strictEqual(actualWarnings, null);
      return;
    }
    assert.strictEqual(actualWarnings.length, warnings);

    if (variant.startsWith('worker')) {
      const workerIds = stderr.match(/\(node:\d+, thread:\d+)/g);
      assert.strictEqual(workerIds.length, warnings);
    }
  }
})().then(common.mustCall());
