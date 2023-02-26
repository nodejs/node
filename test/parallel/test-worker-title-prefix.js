'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const { Worker, isMainThread } = require('worker_threads');

const { Session } = require('inspector');

if (isMainThread) {
  const titlePrefix = 'Hello Thread ';
  new Worker(__filename, {
    workerData: 'Test Worker',
    titlePrefix,
  });

  const session = new Session();
  session.connect();
  session.on('NodeWorker.attachedToWorker', ({ params: { workerInfo } }) => {
    const id = workerInfo.id;
    const expectedTitle = `${titlePrefix}Worker ${id}`;
    assert.strictEqual(workerInfo.title, expectedTitle);
  });
  session.post('NodeWorker.enable', { waitForDebuggerOnStart: false });
}
