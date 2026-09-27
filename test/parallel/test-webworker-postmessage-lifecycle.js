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
