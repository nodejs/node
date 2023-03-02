'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { Worker, isMainThread, parentPort } = require('worker_threads');

const { Session } = require('inspector');

if (isMainThread) {
  const name = 'Hello Thread';
  const worker = new Worker(__filename, {
    name,
  });
  const expectedTitle = `[worker 1] ${name}`;
  worker.once('message', common.mustCall((message) => {
    assert.strictEqual(message, expectedTitle);
    worker.postMessage('done');
  }));
} else {
  const session = new Session();
  parentPort.once('message', () => {});
  session.connectToMainThread();
  session.on('NodeWorker.attachedToWorker', common.mustCall(({ params: { workerInfo } }) => {
    parentPort.postMessage(workerInfo.title);
  }));
  session.post('NodeWorker.enable', { waitForDebuggerOnStart: false });
}
