// Flags: --experimental-web-worker
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { isMainThread } = require('node:worker_threads');
const { pathToFileURL } = require('node:url');

function checkPostMessage(post) {
  // A null iterator selects the dictionary overload.
  const dictionaryBuffer = new ArrayBuffer(8);
  post(null, { [Symbol.iterator]: null, transfer: [dictionaryBuffer] });
  assert.strictEqual(dictionaryBuffer.byteLength, 0);

  // Callable objects can also be iterable transfer lists.
  const functionBuffer = new ArrayBuffer(8);
  function transfer() {}
  transfer[Symbol.iterator] = function*() { yield functionBuffer; };
  post(null, transfer);
  assert.strictEqual(functionBuffer.byteLength, 0);

  // Overload resolution must reuse the iterator method it retrieved.
  const getterBuffer = new ArrayBuffer(8);
  const iterable = {};
  Object.defineProperty(iterable, Symbol.iterator, {
    get: common.mustCall(() => common.mustCall(function*() {
      assert.strictEqual(this, iterable);
      yield getterBuffer;
    })),
  });
  post(null, iterable);
  assert.strictEqual(getterBuffer.byteLength, 0);

  // A present, non-callable iterator must fail before reading the dictionary.
  for (const value of [{}, function() {}]) {
    value[Symbol.iterator] = 1;
    Object.defineProperty(value, 'transfer', { get: common.mustNotCall() });
    assert.throws(() => post(null, value), TypeError);
  }
}

if (isMainThread) {
  const worker = new Worker(pathToFileURL(__filename));
  worker.onerror = common.mustNotCall('worker failed');
  const done = common.mustCall(() => worker.terminate());
  worker.onmessage = ({ data }) => {
    if (data === 'done') done();
  };
  checkPostMessage(worker.postMessage.bind(worker));
} else {
  checkPostMessage(globalThis.postMessage);
  globalThis.postMessage('done');
}
