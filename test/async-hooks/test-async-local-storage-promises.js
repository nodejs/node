'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

async function main() {
  const asyncLocalStorage = new AsyncLocalStorage();
  const err = new Error();
  const next = () => Promise.resolve()
    .then(() => {
      assert.strictEqual(asyncLocalStorage.getStore().get('a'), 1);
      throw err;
    });
  await assert.rejects(new Promise((resolve) => {
    asyncLocalStorage.run(new Map(), () => {
      const store = asyncLocalStorage.getStore();
      store.set('a', 1);
      resolve(next());
    });
  }), err);
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}

main().then(common.mustCall());
