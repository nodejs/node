'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

// Test that workers fail with meaningful error message
// when their initialization fails.

if (common.isWindows) {
  common.skip('ulimit does not work on Windows.');
}

// A reasonably low fd count. An empty node process
// creates around 30 fds for its internal purposes,
// so making it too low will crash the process early,
// making it too high will cause too much resource use.
const OPENFILES = 128;

// Double the open files - so that some workers fail for sure.
const WORKERCOUNT = 256;

if (process.argv[2] === 'child') {
  const { Worker } = require('worker_threads');
  for (let i = 0; i < WORKERCOUNT; ++i) {
    const worker = new Worker(
      'require(\'worker_threads\').parentPort.postMessage(2 + 2)',
      { eval: true });
    worker.on('message', (result) => {
      assert.strictEqual(result, 4);
    });
    worker.on('error', (e) => {
      assert.match(e.message, /Worker initialization failure: EMFILE/);
      assert.strictEqual(e.code, 'ERR_WORKER_INIT_FAILED');
    });
  }

} else {
  // Limit the number of open files, to force workers to fail
  let testCmd = `ulimit -n ${OPENFILES} && `;

  testCmd += `${process.execPath} ${__filename} child`;
  const cp = child_process.exec(testCmd);

  let stdout = '';
  let stderr = '';

  cp.on('exit', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    if (stdout !== '')
      console.log(`child stdout: ${stdout}`);
    if (stderr !== '')
      console.log(`child stderr: ${stderr}`);
  }));

  // Turn on the child streams for debugging purpose
  cp.stderr.on('data', (chunk) => {
    stdout += chunk;
  });
  cp.stdout.on('data', (chunk) => {
    stderr += chunk;
  });
}
