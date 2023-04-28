'use strict';
const common = require('../common');
const assert = require('assert');
const { Worker } = require('worker_threads');
const { once } = require('events');
const helper = require('../common/report');

async function basic() {
  // Test that the report includes basic information about Worker threads.

  const w = new Worker(`
    const { parentPort } = require('worker_threads');
    parentPort.once('message', () => {
      /* Wait for message to stop the Worker */
    });
  `, { eval: true });

  await once(w, 'online');

  const report = process.report.getReport();
  helper.validateContent(report);
  assert.strictEqual(report.workers.length, 1);
  helper.validateContent(report.workers[0]);
  assert.strictEqual(report.workers[0].header.threadId, w.threadId);

  w.postMessage({});

  await once(w, 'exit');
}

async function interruptingJS() {
  // Test that the report also works when Worker threads are busy in JS land.

  const w = new Worker('while (true);', { eval: true });

  await once(w, 'online');

  const report = process.report.getReport();
  helper.validateContent(report);
  assert.strictEqual(report.workers.length, 1);
  helper.validateContent(report.workers[0]);

  await w.terminate();
}

(async function() {
  await basic();
  await interruptingJS();
})().then(common.mustCall());
