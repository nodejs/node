'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs/promises');
const { scheduler } = require('timers/promises');
const { parentPort, Worker } = require('worker_threads');

const MAX_ITERATIONS = 5;
const MAX_THREADS = 6;

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;

  function spinWorker(iter) {
    const w = new Worker(__filename);
    w.on('message', common.mustCall((msg) => {
      assert.strictEqual(msg, 'terminate');
      w.terminate();
    }));

    w.on('exit', common.mustCall(() => {
      if (iter < MAX_ITERATIONS)
        spinWorker(++iter);
    }));
  }

  for (let i = 0; i < MAX_THREADS; i++) {
    spinWorker(0);
  }
} else {
  async function open_nok() {
    await assert.rejects(
      fs.open('this file does not exist'),
      {
        code: 'ENOENT',
        syscall: 'open'
      }
    );
    await scheduler.yield();
    await open_nok();
  }

  // These async function calls never return as they are meant to continually
  // open nonexistent files until the worker is terminated.
  open_nok();
  open_nok();

  parentPort.postMessage('terminate');
}
