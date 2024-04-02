'use strict';
require('../common');
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
  await new Promise((resolve, reject) => {
    asyncLocalStorage.run(new Map(), () => {
      const store = asyncLocalStorage.getStore();
      store.set('a', 1);
      next().then(resolve, reject);
    });
  })
    .catch((e) => {
      assert.strictEqual(asyncLocalStorage.getStore(), undefined);
      assert.strictEqual(e, err);
    });
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}

main();
