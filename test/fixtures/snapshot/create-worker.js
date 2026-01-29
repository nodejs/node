const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;
const assert = require('assert');

setDeserializeMainFunction(() => {
  const { Worker } = require('worker_threads');
  const worker = new Worker(
    'require("worker_threads").parentPort.postMessage({value: 21 + 21})',
    { eval: true });

  const messages = [];
  worker.on('message', (message) => {
    console.log('Worker message:', message);
    messages.push(message);
  });

  process.on('beforeExit', () => {
    console.log('Process beforeExit, messages:', messages);
    assert.deepStrictEqual(messages, [{value:42}]);
  });
});
