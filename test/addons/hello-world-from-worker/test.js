// Flags: --experimental-worker
'use strict';
const assert = require('assert');

function testModule() {
  const common = require('../../common');
  const binding = require(`./build/${common.buildType}/binding`);
  assert.strictEqual(binding(), 'world');
  console.log('binding.hello() =', binding());
}

const { Worker, isMainThread, parentPort } = require('worker_threads');
if (isMainThread) {
  testModule();
  const worker = new Worker(__filename);
  worker.on('message', (message) => {
    assert.strictEqual(message, undefined);
    worker.terminate();
  });
  worker.on('error', (error) => {
    console.error(error);
    worker.terminate();
  });
} else {
  let response;
  try {
    testModule();
  } catch (e) {
    response = e + '';
  }
  parentPort.postMessage(response);
}
