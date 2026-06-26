// Flags: --async-context-frame
'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();
const sensitive = { secret: 'sensitive' };
let toPrimitiveStore = 'not called';
let downstreamStore = 'not called';

const thrown = {
  [Symbol.toPrimitive]: common.mustCall(() => {
    toPrimitiveStore = asyncLocalStorage.getStore();
    queueMicrotask(common.mustCall(() => {
      downstreamStore = asyncLocalStorage.getStore();
      assert.strictEqual(downstreamStore, undefined);
    }));
    return 'thrown';
  }),
};

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err, thrown);
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}));

asyncLocalStorage.run(sensitive, () => {
  queueMicrotask(() => {
    throw thrown;
  });
});

setImmediate(common.mustCall(() => {
  assert.strictEqual(toPrimitiveStore, undefined);
  assert.strictEqual(downstreamStore, undefined);
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}));
