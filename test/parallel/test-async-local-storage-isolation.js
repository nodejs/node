'use strict';
const common = require('../common');
const { AsyncLocalStorage } = require('node:async_hooks');
const assert = require('node:assert');

// Verify that ALS instances are independent of each other.

{
  const als1 = new AsyncLocalStorage();
  const als2 = new AsyncLocalStorage();

  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), undefined);

  als1.run('store1', common.mustCall(() => {
    assert.strictEqual(als1.getStore(), 'store1');
    assert.strictEqual(als2.getStore(), undefined);

    als2.run('store2', common.mustCall(() => {
      assert.strictEqual(als1.getStore(), 'store1');
      assert.strictEqual(als2.getStore(), 'store2');
    }));
    assert.strictEqual(als1.getStore(), 'store1');
    assert.strictEqual(als2.getStore(), undefined);

    als2.enterWith('store3');
    assert.strictEqual(als1.getStore(), 'store1');
    assert.strictEqual(als2.getStore(), 'store3');
  }));

  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), 'store3');
}

{
  const als1 = new AsyncLocalStorage();
  const als2 = new AsyncLocalStorage();
  const als3 = new AsyncLocalStorage();

  als3.enterWith('store3');

  als1.run('store1', common.mustCall(() => {
    assert.strictEqual(als1.getStore(), 'store1');
    assert.strictEqual(als2.getStore(), undefined);
    assert.strictEqual(als3.getStore(), 'store3');

    als2.run('store2', common.mustCall(() => {
      assert.strictEqual(als1.getStore(), 'store1');
      assert.strictEqual(als2.getStore(), 'store2');
      assert.strictEqual(als3.getStore(), 'store3');

      als1.disable();
      assert.strictEqual(als1.getStore(), undefined);
      assert.strictEqual(als2.getStore(), 'store2');
      assert.strictEqual(als3.getStore(), 'store3');
    }));
    assert.strictEqual(als1.getStore(), undefined);
    assert.strictEqual(als2.getStore(), undefined);
    assert.strictEqual(als3.getStore(), 'store3');
  }));

  assert.strictEqual(als1.getStore(), undefined);
  assert.strictEqual(als2.getStore(), undefined);
  assert.strictEqual(als3.getStore(), 'store3');
}
