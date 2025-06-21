import test from 'node:test';

test('deeply nested tests', async (t) => {
  t.plan(1);

  await t.test('level 1', async (t) => {
    t.plan(1);

    await t.test('level 2', (t) => {
      t.plan(1);
      t.assert.ok(true, 'deepest assertion');
    });
  });
});
