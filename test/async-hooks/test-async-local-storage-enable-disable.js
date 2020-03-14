'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.run(new Map(), () => {
  asyncLocalStorage.getStore().set('foo', 'bar');
  process.nextTick(() => {
    assert.strictEqual(asyncLocalStorage.getStore().get('foo'), 'bar');
    process.nextTick(() => {
      assert.strictEqual(asyncLocalStorage.getStore(), undefined);
    });

    asyncLocalStorage.disable();
    assert.strictEqual(asyncLocalStorage.getStore(), undefined);

    // Calls to exit() should not mess with enabled status
    asyncLocalStorage.exit(() => {
      assert.strictEqual(asyncLocalStorage.getStore(), undefined);
    });
    assert.strictEqual(asyncLocalStorage.getStore(), undefined);

    process.nextTick(() => {
      assert.strictEqual(asyncLocalStorage.getStore(), undefined);
      asyncLocalStorage.run(new Map(), () => {
        assert.notStrictEqual(asyncLocalStorage.getStore(), undefined);
      });
    });
  });
});
