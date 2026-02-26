/* eslint-disable no-unused-vars */
'use strict';
require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');

// Test basic RunScope with using
{
  const storage = new AsyncLocalStorage();

  assert.strictEqual(storage.getStore(), undefined);

  {
    using scope = storage.withScope('test');
    assert.strictEqual(storage.getStore(), 'test');
  }

  // Store should be restored to undefined
  assert.strictEqual(storage.getStore(), undefined);
}

// Test RunScope restores previous value
{
  const storage = new AsyncLocalStorage();

  storage.enterWith('initial');
  assert.strictEqual(storage.getStore(), 'initial');

  {
    using scope = storage.withScope('scoped');
    assert.strictEqual(storage.getStore(), 'scoped');
  }

  // Should restore to previous value
  assert.strictEqual(storage.getStore(), 'initial');
}

// Test nested RunScope
{
  const storage = new AsyncLocalStorage();
  const storeValues = [];

  {
    using outer = storage.withScope('outer');
    storeValues.push(storage.getStore());

    {
      using inner = storage.withScope('inner');
      storeValues.push(storage.getStore());
    }

    // Should restore to outer
    storeValues.push(storage.getStore());
  }

  // Should restore to undefined
  storeValues.push(storage.getStore());

  assert.deepStrictEqual(storeValues, ['outer', 'inner', 'outer', undefined]);
}

// Test RunScope with error during usage
{
  const storage = new AsyncLocalStorage();

  storage.enterWith('before');

  const testError = new Error('test');

  assert.throws(() => {
    using scope = storage.withScope('during');
    assert.strictEqual(storage.getStore(), 'during');
    throw testError;
  }, testError);

  // Store should be restored even after error
  assert.strictEqual(storage.getStore(), 'before');
}

// Test idempotent disposal
{
  const storage = new AsyncLocalStorage();

  const scope = storage.withScope('test');
  assert.strictEqual(storage.getStore(), 'test');

  // Dispose via Symbol.dispose
  scope[Symbol.dispose]();
  assert.strictEqual(storage.getStore(), undefined);

  storage.enterWith('test2');
  assert.strictEqual(storage.getStore(), 'test2');

  // Double dispose should be idempotent
  scope[Symbol.dispose]();
  assert.strictEqual(storage.getStore(), 'test2');
}

// Test RunScope with defaultValue
{
  const storage = new AsyncLocalStorage({ defaultValue: 'default' });

  assert.strictEqual(storage.getStore(), 'default');

  {
    using scope = storage.withScope('custom');
    assert.strictEqual(storage.getStore(), 'custom');
  }

  // Should restore to default
  assert.strictEqual(storage.getStore(), 'default');
}

// Test deeply nested RunScope
{
  const storage = new AsyncLocalStorage();

  {
    using s1 = storage.withScope(1);
    assert.strictEqual(storage.getStore(), 1);

    {
      using s2 = storage.withScope(2);
      assert.strictEqual(storage.getStore(), 2);

      {
        using s3 = storage.withScope(3);
        assert.strictEqual(storage.getStore(), 3);

        {
          using s4 = storage.withScope(4);
          assert.strictEqual(storage.getStore(), 4);
        }

        assert.strictEqual(storage.getStore(), 3);
      }

      assert.strictEqual(storage.getStore(), 2);
    }

    assert.strictEqual(storage.getStore(), 1);
  }

  assert.strictEqual(storage.getStore(), undefined);
}

// Test RunScope with multiple storages
{
  const storage1 = new AsyncLocalStorage();
  const storage2 = new AsyncLocalStorage();

  {
    using scope1 = storage1.withScope('A');

    {
      using scope2 = storage2.withScope('B');

      assert.strictEqual(storage1.getStore(), 'A');
      assert.strictEqual(storage2.getStore(), 'B');
    }

    assert.strictEqual(storage1.getStore(), 'A');
    assert.strictEqual(storage2.getStore(), undefined);
  }

  assert.strictEqual(storage1.getStore(), undefined);
  assert.strictEqual(storage2.getStore(), undefined);
}
