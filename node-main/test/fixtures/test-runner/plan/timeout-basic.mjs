import test from 'node:test';

test('planning with wait should PASS within timeout', async (t) => {
  t.plan(1, { wait: 5000 });
  setTimeout(() => {
    t.assert.ok(true);
  }, 250);
});

test('planning with wait should FAIL within timeout', async (t) => {
  t.plan(1, { wait: 5000 });
  setTimeout(() => {
    t.assert.ok(false);
  }, 250);
});
