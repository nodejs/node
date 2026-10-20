// Flags: --experimental-web-worker
'use strict';

const common = require('../common');
const assert = require('node:assert');

function checkSerialization(worker) {
  assert.throws(() => worker.postMessage(() => {}), { name: 'DataCloneError' });
  const buffer = new ArrayBuffer(8);
  assert.throws(() => worker.postMessage(null, [buffer, buffer]), { name: 'DataCloneError' });
  assert.strictEqual(buffer.byteLength, 8);
  worker.postMessage(null, [buffer]);
  assert.strictEqual(buffer.byteLength, 0);
}

// A failed script fetch still leaves an outside port that serializes messages.
{
  const worker = new Worker('data:text/plain,');
  worker.onerror = common.mustCall(() => checkSerialization(worker));
  checkSerialization(worker);
}

// Serialization is required even after the backing thread has exited.
{
  const worker = new Worker('data:text/javascript,close()');
  worker.onerror = common.mustNotCall('worker failed');
  process.once('worker', common.mustCall((thread) => {
    thread.once('exit', common.mustCall(() => checkSerialization(worker)));
  }));
}

// Queue messages before terminating so the test does not depend on thread speed.
{
  const source = `
    onmessage = ({ data }) => {
      for (let i = 0; i < 3; i++) postMessage(i);
      const state = new Int32Array(data);
      Atomics.store(state, 0, 1);
      Atomics.notify(state, 0);
    };
  `;
  const worker = new Worker(`data:text/javascript,${encodeURIComponent(source)}`);
  worker.onerror = common.mustNotCall('worker failed');
  worker.onmessage = common.mustNotCall('message delivered after terminate()');
  const state = new Int32Array(new SharedArrayBuffer(4));
  worker.postMessage(state.buffer);
  assert.notStrictEqual(Atomics.wait(state, 0, 0, common.platformTimeout(10000)), 'timed-out');
  worker.terminate();
  worker.terminate();
  checkSerialization(worker);
}

// terminate() can run during message dispatch, including while the backing
// thread is draining messages on exit. Finish this event but discard later ones.
for (const closeAfterPosting of [false, true]) {
  const source = `
    onmessage = ({ data }) => {
      for (let i = 0; i < 3; i++) postMessage(i);
      const state = new Int32Array(data);
      Atomics.store(state, 0, 1);
      Atomics.notify(state, 0);
      if (${closeAfterPosting}) close();
    };
  `;
  const worker = new Worker(`data:text/javascript,${encodeURIComponent(source)}`);
  worker.onerror = common.mustNotCall('worker failed');
  worker.addEventListener('message', common.mustCall(({ data }) => {
    assert.strictEqual(data, 0);
    worker.terminate();
    checkSerialization(worker);
  }));
  worker.addEventListener('message', common.mustCall(({ data }) => {
    assert.strictEqual(data, 0);
  }));
  const state = new Int32Array(new SharedArrayBuffer(4));
  worker.postMessage(state.buffer);
  assert.notStrictEqual(Atomics.wait(state, 0, 0, common.platformTimeout(10000)), 'timed-out');
}
