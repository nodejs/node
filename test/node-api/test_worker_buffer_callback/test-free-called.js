'use strict';
// Addons: binding, binding_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const path = require('path');
const assert = require('assert');
const { Worker } = require('worker_threads');
const binding = path.resolve(__dirname, addonPath);
const { getFreeCallCount } = require(binding);

// Test that buffers allocated with a free callback through our APIs are
// released when a Worker owning it exits.

const w = new Worker(`require(${JSON.stringify(binding)})`, { eval: true });

assert.strictEqual(getFreeCallCount(), 0);
w.on('exit', common.mustCall(() => {
  assert.strictEqual(getFreeCallCount(), 1);
}));
