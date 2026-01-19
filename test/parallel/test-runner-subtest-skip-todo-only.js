'use strict';
require('../common');
const assert = require('node:assert');
const { test } = require('node:test');
const { isPromise } = require('node:util/types');

// Regression test for https://github.com/nodejs/node/issues/50665
// Ensures that t.test.skip, t.test.todo, and t.test.only are available
// on the subtest context.

test('subtest context should have test variants', async (t) => {
  assert.strictEqual(typeof t.test, 'function');
  assert.strictEqual(typeof t.test.skip, 'function');
  assert.strictEqual(typeof t.test.todo, 'function');
  assert.strictEqual(typeof t.test.only, 'function');
});

test('subtest variants should return promises', async (t) => {
  const normal = t.test('normal subtest');
  const skipped = t.test.skip('skipped subtest');
  const todo = t.test.todo('todo subtest');

  assert(isPromise(normal));
  assert(isPromise(skipped));
  assert(isPromise(todo));

  await normal;
  await skipped;
  await todo;
});

test('nested subtests should have test variants', async (t) => {
  await t.test('level 1', async (t) => {
    assert.strictEqual(typeof t.test, 'function');
    assert.strictEqual(typeof t.test.skip, 'function');
    assert.strictEqual(typeof t.test.todo, 'function');
    assert.strictEqual(typeof t.test.only, 'function');

    await t.test('level 2', async (t) => {
      assert.strictEqual(typeof t.test, 'function');
      assert.strictEqual(typeof t.test.skip, 'function');
      assert.strictEqual(typeof t.test.todo, 'function');
      assert.strictEqual(typeof t.test.only, 'function');
    });
  });
});
