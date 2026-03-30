'use strict';
const common = require('../common');

const { Worker } = require('worker_threads');
const { createHook } = require('async_hooks');
const assert = require('assert');

let handle;

createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (type === 'WORKER') {
      handle = resource;
      this.disable();
    }
  }
}).enable();

const w = new Worker('', { eval: true });

assert.strictEqual(handle.hasRef(), true);
w.unref();
assert.strictEqual(handle.hasRef(), false);
w.ref();
assert.strictEqual(handle.hasRef(), true);

w.on('exit', common.mustCall((exitCode) => {
  assert.strictEqual(exitCode, 0);
  assert.strictEqual(handle.hasRef(), true);
  setTimeout(common.mustCall(() => {
    assert.strictEqual(handle.hasRef(), undefined);
  }), 0);
}));
