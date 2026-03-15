'use strict';

require('../common');
const assert = require('node:assert');
const { test } = require('node:test');
const { isPromise } = require('node:util/types');

test('subtest context should have test variants', async (t) => {
  assert.strictEqual(typeof t.test, 'function');
  assert.strictEqual(typeof t.test.expectFailure, 'function');
  assert.strictEqual(typeof t.test.only, 'function');
  assert.strictEqual(typeof t.test.skip, 'function');
  assert.strictEqual(typeof t.test.todo, 'function');
});

test('subtest should return a promise', async (t) => {
  const normal = t.test('normal subtest');
  assert.ok(isPromise(normal));
  await normal;
});

test('t.test[variant]() should return a promise', async (t) => {
  assert.ok(isPromise(
    t.test.expectFailure('expectFailure subtest', () => { throw new Error('This should pass'); })
  ));
  assert.ok(isPromise(
    t.test.only('only subtest')
  ));
  assert.ok(isPromise(
    t.test.skip('skip subtest')
  ));
  assert.ok(isPromise(
    t.test.todo('todo subtest')
  ));
});

test('nested subtests should have test variants', async (t) => {
  await t.test('level 1', async (t) => {
    assert.strictEqual(typeof t.test, 'function');
    assert.strictEqual(typeof t.test.expectFailure, 'function');
    assert.strictEqual(typeof t.test.only, 'function');
    assert.strictEqual(typeof t.test.skip, 'function');
    assert.strictEqual(typeof t.test.todo, 'function');

    await t.test('level 2', async (t) => {
      assert.strictEqual(typeof t.test, 'function');
      assert.strictEqual(typeof t.test.expectFailure, 'function');
      assert.strictEqual(typeof t.test.skip, 'function');
      assert.strictEqual(typeof t.test.todo, 'function');
      assert.strictEqual(typeof t.test.only, 'function');
    });
  });
});
