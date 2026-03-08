'use strict';
const common = require('../common');
const assert = require('node:assert');
const { test } = require('node:test');

// Verify that all subtest variants exist on TestContext.
test('subtest variants exist on TestContext', common.mustCall(async (t) => {
  assert.strictEqual(typeof t.test, 'function');
  assert.strictEqual(typeof t.test.skip, 'function');
  assert.strictEqual(typeof t.test.todo, 'function');
  assert.strictEqual(typeof t.test.only, 'function');
  assert.strictEqual(typeof t.test.expectFailure, 'function');
}));

// t.test.skip: callback must NOT be called.
test('t.test.skip prevents callback execution', common.mustCall(async (t) => {
  await t.test.skip('skipped subtest', common.mustNotCall());
}));

// t.test.todo without callback: subtest is marked as todo and skipped.
test('t.test.todo without callback', common.mustCall(async (t) => {
  await t.test.todo('todo subtest without callback');
}));

// t.test.todo with callback: callback runs but subtest is marked as todo.
test('t.test.todo with callback runs the callback', common.mustCall(async (t) => {
  await t.test.todo('todo subtest with callback', common.mustCall());
}));

// Plan counting works with subtest variants.
test('plan counts subtest variants', common.mustCall(async (t) => {
  t.plan(3);
  await t.test('normal subtest', common.mustCall());
  await t.test.skip('skipped subtest');
  await t.test.todo('todo subtest');
}));

// Nested subtests also expose the variants.
test('nested subtests have variants', common.mustCall(async (t) => {
  await t.test('level 1', common.mustCall(async (t2) => {
    assert.strictEqual(typeof t2.test.skip, 'function');
    assert.strictEqual(typeof t2.test.todo, 'function');
    assert.strictEqual(typeof t2.test.only, 'function');
    assert.strictEqual(typeof t2.test.expectFailure, 'function');

    await t2.test.skip('nested skipped', common.mustNotCall());
  }));
}));
