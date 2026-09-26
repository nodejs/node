// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const { Worker, isMainThread, workerData } = require('worker_threads');
const { internalBinding } = require('internal/test/binding');

const contextFromSnapshot =
  process.config.variables.node_use_node_snapshot &&
  !process.execArgv.includes('--no-node-snapshot') &&
  (isMainThread || !process.execArgv.includes('--no-worker-snapshot'));
const { compiledInSnapshot } = internalBinding('builtins').getCacheUsage();
for (const id of ['stream', 'internal/streams/readable', 'internal/streams/writable']) {
  assert.strictEqual(compiledInSnapshot.includes(id), Boolean(contextFromSnapshot), id);
}

const { Readable, Writable, getDefaultHighWaterMark, setDefaultHighWaterMark } = require('stream');

if (isMainThread) {
  const highWaterMark = getDefaultHighWaterMark(false);
  // Runtime changes in the parent must not become the worker's initial state.
  setDefaultHighWaterMark(false, highWaterMark + 1);
  Readable.prototype.parentOnly = true;
  const worker = new Worker(__filename, { workerData: { highWaterMark } });
  worker.on('error', common.mustNotCall());
  worker.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
} else {
  assert.strictEqual(Readable.prototype.parentOnly, undefined);
  if (workerData?.highWaterMark !== undefined) {
    assert.strictEqual(getDefaultHighWaterMark(false), workerData.highWaterMark);
  }
  assert(process.stdin instanceof Readable);
  assert(process.stdout instanceof Writable);
  assert(process.stderr instanceof Writable);
  assert.strictEqual(typeof process.stdin.map, 'function');
  assert.strictEqual(typeof process.stdin.toArray, 'function');
  Readable.from([1, 2, 3]).map((value) => value * 2).toArray().then((values) => {
    assert.deepStrictEqual(values, [2, 4, 6]);
  }).then(common.mustCall());
}
