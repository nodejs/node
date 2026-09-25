import test from 'node:test';

test('parent test', async (t) => {
  t.plan(1);
  await t.test('child test', (t) => {
    t.plan(1);
    t.assert.ok(true, 'child assertion');
  });
});
