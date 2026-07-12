'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();
const { isMainThread, Worker } = require('worker_threads');
const assert = require('assert');
const inspector = require('inspector');

if (!isMainThread) {
  // Verify the inspector api on the worker thread.
  assert.strictEqual(inspector.url(), undefined);

  inspector.open(0, undefined, false);
  const wsUrl = inspector.url();
  assert(wsUrl.startsWith('ws://'));
  inspector.close();
  assert.strictEqual(inspector.url(), undefined);
  return;
}

// Open inspector on the main thread first.
inspector.open(0, undefined, false);
const wsUrl = inspector.url();
assert(wsUrl.startsWith('ws://'));

const worker = new Worker(__filename);
worker.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);

  // Verify inspector on the main thread is still active.
  assert.strictEqual(inspector.url(), wsUrl);
  inspector.close();
  assert.strictEqual(inspector.url(), undefined);
}));
