// Flags: --expose-gc
'use strict';
const common = require('../common');
const assert = require('node:assert');
const zlib = require('node:zlib');
const v8 = require('node:v8');
const { AsyncLocalStorage } = require('node:async_hooks');

// This test verifies that referencing an AsyncLocalStorage store from
// a weak AsyncWrap does not prevent the store from being garbage collected.
// We use zlib streams as examples of weak AsyncWraps here, but the
// issue is not specific to zlib.

class Store {}

let zlibDone = false;

// Use immediates to ensure no accidental async context propagation occurs
setImmediate(common.mustCall(() => {
  const asyncLocalStorage = new AsyncLocalStorage();
  const store = new Store();
  asyncLocalStorage.run(store, common.mustCall(() => {
    (async () => {
      const zlibStream = zlib.createGzip();
      // (Make sure this test does not break if _handle is renamed
      // to something else)
      assert.strictEqual(typeof zlibStream._handle, 'object');
      // Create backreference from AsyncWrap to store
      store.zlibStream = zlibStream._handle;
      // Let the zlib stream finish (not strictly necessary)
      zlibStream.end('hello world');
      await zlibStream.toArray();
      zlibDone = true;
    })().then(common.mustCall());
  }));
}));

const finish = common.mustCall(() => {
  global.gc();
  // Make sure the ALS instance has been garbage-collected
  assert.strictEqual(v8.queryObjects(Store), 0);
});

const interval = setInterval(() => {
  if (zlibDone) {
    clearInterval(interval);
    finish();
  }
}, 5);
