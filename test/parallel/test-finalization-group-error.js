'use strict';

// Flags: --expose-gc --harmony-weak-refs

const common = require('../common');
const assert = require('assert');

const g = new globalThis.FinalizationGroup(common.mustCallAtLeast(() => {
  throw new Error('test');
}, 1));
g.register({}, 42);

setTimeout(() => {
  globalThis.gc();
  assert.throws(() => {
    g.cleanupSome();
  }, {
    name: 'Error',
    message: 'test',
  });
}, 200);

process.on('uncaughtException', common.mustCall());
