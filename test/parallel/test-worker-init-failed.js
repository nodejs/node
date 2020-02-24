'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');

// Test that workers fail with meaningful error message
// when their initialization fails.

if (process.argv[2] === 'child') {
  const {Worker} = require('worker_threads');
  for (let i = 0; i < 256; ++i) {
    const worker = new Worker(
      'require(\'worker_threads\').parentPort.postMessage(2 + 2)',
      { eval: true });
    worker.on('message', (result) => {
      assert.strictEqual(result, 4);
    })
    worker.on('error', (e) => {
      assert(e.message.match(/Worker initialization failure: EMFILE/));
      assert.strictEqual(e.code, 'ERR_WORKER_INIT_FAILED');
    })
  }

} else {

  if (common.isWindows) {
    common.skip('ulimit does not work on Windows.');
  }

  // limit the number of open files, to force workers to fail
  let testCmd = 'ulimit -n 128 && ';

  testCmd += `${process.argv[0]} ${process.argv[1]} child`;
  const cp = child_process.exec(testCmd);

  // turn on the child streams for debugging purpose
  cp.stderr.on('data', (d) => {
    console.log(d.toString());
  })
  cp.stdout.on('data', (d) => {
    console.log(d.toString());
  })
}
