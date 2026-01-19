const {
  setDeserializeMainFunction,
} = require('v8').startupSnapshot;
const assert = require('assert');

setDeserializeMainFunction(() => {
  const vm = require('vm');
  const { Worker } = require('worker_threads');
  assert.strictEqual(vm.runInNewContext('21+21'), 42);
  const worker = new Worker(
    'require("worker_threads").parentPort.postMessage({value: 21 + 21})',
    { eval: true });

  const messages = [];
  worker.on('message', message => messages.push(message));

  process.on('beforeExit', () => {
    assert.deepStrictEqual(messages, [{value:42}]);
  })
});
