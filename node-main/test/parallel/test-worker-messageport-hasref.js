'use strict';
const common = require('../common');

const { Worker } = require('worker_threads');
const { createHook } = require('async_hooks');
const { deepStrictEqual, strictEqual } = require('assert');

const m = new Map();
createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (['WORKER', 'MESSAGEPORT'].includes(type)) {
      m.set(asyncId, { type, resource });
    }
  },
  destroy(asyncId) {
    m.delete(asyncId);
  }
}).enable();

function getActiveWorkerAndMessagePortTypes() {
  const activeWorkerAndMessagePortTypes = [];
  for (const asyncId of m.keys()) {
    const { type, resource } = m.get(asyncId);
    // Same logic as https://github.com/mafintosh/why-is-node-running/blob/24fb4c878753390a05d00959e6173d0d3c31fddd/index.js#L31-L32.
    if (typeof resource.hasRef !== 'function' || resource.hasRef() === true) {
      activeWorkerAndMessagePortTypes.push(type);
    }
  }
  return activeWorkerAndMessagePortTypes;
}

const w = new Worker('', { eval: true });
deepStrictEqual(getActiveWorkerAndMessagePortTypes(), ['WORKER']);
w.unref();
deepStrictEqual(getActiveWorkerAndMessagePortTypes(), []);
w.ref();
deepStrictEqual(getActiveWorkerAndMessagePortTypes(), ['WORKER', 'MESSAGEPORT']);

w.on('exit', common.mustCall((exitCode) => {
  strictEqual(exitCode, 0);
  deepStrictEqual(getActiveWorkerAndMessagePortTypes(), ['WORKER']);
  setTimeout(common.mustCall(() => {
    deepStrictEqual(getActiveWorkerAndMessagePortTypes(), []);
  }), 0);
}));
