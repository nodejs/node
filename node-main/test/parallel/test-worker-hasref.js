'use strict';
const common = require('../common');

const { Worker } = require('worker_threads');
const { createHook } = require('async_hooks');
const { strictEqual } = require('assert');

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

strictEqual(handle.hasRef(), true);
w.unref();
strictEqual(handle.hasRef(), false);
w.ref();
strictEqual(handle.hasRef(), true);

w.on('exit', common.mustCall((exitCode) => {
  strictEqual(exitCode, 0);
  strictEqual(handle.hasRef(), true);
  setTimeout(common.mustCall(() => {
    strictEqual(handle.hasRef(), undefined);
  }), 0);
}));
