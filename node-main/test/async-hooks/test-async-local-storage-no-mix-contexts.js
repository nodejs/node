'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();
const asyncLocalStorage2 = new AsyncLocalStorage();

setTimeout(common.mustCall(() => {
  asyncLocalStorage.run(new Map(), common.mustCall(() => {
    asyncLocalStorage2.run(new Map(), common.mustCall(() => {
      const store = asyncLocalStorage.getStore();
      const store2 = asyncLocalStorage2.getStore();
      store.set('hello', 'world');
      store2.set('hello', 'foo');
      setTimeout(common.mustCall(() => {
        assert.strictEqual(asyncLocalStorage.getStore().get('hello'), 'world');
        assert.strictEqual(asyncLocalStorage2.getStore().get('hello'), 'foo');
        asyncLocalStorage.exit(common.mustCall(() => {
          assert.strictEqual(asyncLocalStorage.getStore(), undefined);
          assert.strictEqual(asyncLocalStorage2.getStore().get('hello'), 'foo');
        }));
        assert.strictEqual(asyncLocalStorage.getStore().get('hello'), 'world');
        assert.strictEqual(asyncLocalStorage2.getStore().get('hello'), 'foo');
      }), 200);
    }));
  }));
}), 100);

setTimeout(common.mustCall(() => {
  asyncLocalStorage.run(new Map(), common.mustCall(() => {
    const store = asyncLocalStorage.getStore();
    store.set('hello', 'earth');
    setTimeout(common.mustCall(() => {
      assert.strictEqual(asyncLocalStorage.getStore().get('hello'), 'earth');
    }), 100);
  }));
}), 100);
