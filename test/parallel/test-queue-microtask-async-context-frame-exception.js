// Flags: --async-context-frame
'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();
const sensitive = { secret: 'sensitive' };
let formattingStore = 'not called';
let downstreamStore = 'not called';

const thrown = {
  [Symbol.toPrimitive]: common.mustCall(() => {
    // Exception formatting re-enters JavaScript while the throwing
    // microtask's context frame is still current, so the context is
    // available here.
    formattingStore = asyncLocalStorage.getStore();
    // A microtask queued while the frame is still current captures that
    // context.
    queueMicrotask(common.mustCall(() => {
      downstreamStore = asyncLocalStorage.getStore();
      assert.strictEqual(downstreamStore, sensitive);
    }));
    return 'thrown';
  }),
};

process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err, thrown);
  // The AsyncLocalStorage context of the throwing microtask is still
  // available to the uncaughtException handler.
  assert.strictEqual(asyncLocalStorage.getStore(), sensitive);
}));

asyncLocalStorage.run(sensitive, () => {
  queueMicrotask(() => {
    throw thrown;
  });
});

setImmediate(common.mustCall(() => {
  // Once the microtask queue has drained, the context frame is no longer
  // current.
  assert.strictEqual(formattingStore, sensitive);
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}));
