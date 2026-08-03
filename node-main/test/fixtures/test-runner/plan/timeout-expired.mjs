import test from 'node:test';

test('planning should FAIL when wait time expires before plan is met', (t) => {
  t.plan(2, { wait: 500 });
  setTimeout(() => {
    t.assert.ok(true);
  }, 30_000).unref();
});
