'use strict';

const common = require('../common');
const assert = require('assert');
const { RawSession } = require('inspector');
const { Worker, isMainThread, workerData } = require('worker_threads');

common.skipIfInspectorDisabled();

if (!workerData) {
  common.skipIfWorker();
}

if (isMainThread) {
  new Worker(__filename, { workerData: {} });
} else {
  const session = new RawSession();
  session.connectToMainThread();
  session.on('message', (result) => {
    const data = JSON.parse(result);
    assert.ok(data.id === 1);
    assert.ok(data.result.result.type === 'number');
    assert.ok(data.result.result.value === 2);
  });

  const data = JSON.stringify({
    method: 'Runtime.evaluate',
    id: 1,
    params: { expression: '1 + 1' }
  });

  session.post(data);
}
