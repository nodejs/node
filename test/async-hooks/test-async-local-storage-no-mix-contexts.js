'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();
const asyncLocalStorage2 = new AsyncLocalStorage();

setTimeout(() => {
  asyncLocalStorage.run(new Map(), () => {
    asyncLocalStorage2.run(new Map(), () => {
      const store = asyncLocalStorage.getStore();
      const store2 = asyncLocalStorage2.getStore();
      store.set('hello', 'world');
      store2.set('hello', 'foo');
      setTimeout(() => {
        assert.strictEqual(asyncLocalStorage.getStore().get('hello'), 'world');
        assert.strictEqual(asyncLocalStorage2.getStore().get('hello'), 'foo');
        asyncLocalStorage.exit(() => {
          assert.strictEqual(asyncLocalStorage.getStore(), undefined);
          assert.strictEqual(asyncLocalStorage2.getStore().get('hello'), 'foo');
        });
        assert.strictEqual(asyncLocalStorage.getStore().get('hello'), 'world');
        assert.strictEqual(asyncLocalStorage2.getStore().get('hello'), 'foo');
      }, 200);
    });
  });
}, 100);

setTimeout(() => {
  asyncLocalStorage.run(new Map(), () => {
    const store = asyncLocalStorage.getStore();
    store.set('hello', 'earth');
    setTimeout(() => {
      assert.strictEqual(asyncLocalStorage.getStore().get('hello'), 'earth');
    }, 100);
  });
}, 100);
