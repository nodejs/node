'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');

// Attempts to test that the source map JS code run on process shutdown
// does not call any user-defined JS code.

const { Worker, workerData, parentPort } = require('worker_threads');

if (!workerData) {
  tmpdir.refresh();
  process.env.NODE_V8_COVERAGE = tmpdir.path;

  // Count the number of some calls that should not be made.
  const callCount = new Int32Array(new SharedArrayBuffer(4));
  const w = new Worker(__filename, { workerData: { callCount } });
  w.on('message', common.mustCall(() => w.terminate()));
  w.on('exit', common.mustCall(() => {
    assert.strictEqual(callCount[0], 0);
  }));
  return;
}

const { callCount } = workerData;

function increaseCallCount() { callCount[0]++; }

// Increase the call count when a forbidden method is called.
for (const property of ['_cache', 'lineLengths', 'url']) {
  Object.defineProperty(Object.prototype, property, {
    get: increaseCallCount,
    set: increaseCallCount
  });
}
Object.getPrototypeOf([][Symbol.iterator]()).next = increaseCallCount;
Object.getPrototypeOf((new Map()).entries()).next = increaseCallCount;
Array.prototype[Symbol.iterator] = increaseCallCount;
Map.prototype[Symbol.iterator] = increaseCallCount;
Map.prototype.entries = increaseCallCount;
Object.keys = increaseCallCount;
Object.create = increaseCallCount;
Object.hasOwnProperty = increaseCallCount;

parentPort.postMessage('done');
