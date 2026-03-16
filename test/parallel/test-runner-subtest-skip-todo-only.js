'use strict';
// Regression test for https://github.com/nodejs/node/issues/50665
// t.test() should expose the same .skip, .todo, .only, .expectFailure
// variants as the top-level test() function.
const common = require('../common');
const assert = require('node:assert');
const { test } = require('node:test');

test('context test function exposes all variant methods', async (t) => {
  for (const variant of ['skip', 'todo', 'only', 'expectFailure']) {
    assert.strictEqual(
      typeof t.test[variant], 'function',
      `t.test.${variant} should be a function`,
    );
  }
});

test('skip variant does not execute the callback', async (t) => {
  await t.test.skip('this is skipped', common.mustNotCall());
});

test('todo variant without callback marks subtest as todo', async (t) => {
  await t.test.todo('pending feature');
});

test('todo variant with callback still runs', async (t) => {
  let ran = false;
  await t.test.todo('work in progress', () => { ran = true; });
  assert.ok(ran, 'todo callback should have been invoked');
});

test('variants increment the plan counter', async (t) => {
  t.plan(4);
  await t.test('regular', common.mustCall());
  await t.test.skip('skipped');
  await t.test.todo('todo');
  await t.test.todo('todo with cb', common.mustCall());
});

test('variants propagate to nested subtests', async (t) => {
  await t.test('outer', async (t2) => {
    for (const variant of ['skip', 'todo', 'only', 'expectFailure']) {
      assert.strictEqual(
        typeof t2.test[variant], 'function',
        `nested t.test.${variant} should be a function`,
      );
    }
    await t2.test.skip('inner skip', common.mustNotCall());
  });
});
