'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run(new Map(), common.mustCall(() => {
  asyncLocalStorage.getStore().set('foo', 'bar');
  process.nextTick(() => {
    assert.strictEqual(asyncLocalStorage.getStore().get('foo'), 'bar');
    process.nextTick(() => {
      assert.strictEqual(asyncLocalStorage.getStore(), undefined);
    });

    asyncLocalStorage.disable();
    assert.strictEqual(asyncLocalStorage.getStore(), undefined);

    // Calls to exit() should not mess with enabled status
    asyncLocalStorage.exit(common.mustCall(() => {
      assert.strictEqual(asyncLocalStorage.getStore(), undefined);
    }));
    assert.strictEqual(asyncLocalStorage.getStore(), undefined);

    process.nextTick(() => {
      assert.strictEqual(asyncLocalStorage.getStore(), undefined);
      asyncLocalStorage.run(new Map().set('bar', 'foo'), common.mustCall(() => {
        assert.strictEqual(asyncLocalStorage.getStore().get('bar'), 'foo');
      }));
    });
  });
}));
