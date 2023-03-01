'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');

const { Session } = require('inspector');

if (isMainThread) {
  const name = 'Hello Thread';
  new Worker(__filename, {
    workerData: 'Test Worker',
    name,
  });

  const session = new Session();
  session.connect();
  session.on('NodeWorker.attachedToWorker', common.mustCall(({ params: { workerInfo } }) => {
    const id = workerInfo.workerId;
    const expectedTitle = `[Worker ${id}] ${name}`;
    assert.strictEqual(workerInfo.title, expectedTitle);
  }));
  session.post('NodeWorker.enable', { waitForDebuggerOnStart: false });
} else {
  console.log('inside worker');
}
