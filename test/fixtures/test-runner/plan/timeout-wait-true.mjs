import test from 'node:test';

test('should pass when assertions are eventually met', async (t) => {
  t.plan(1, { wait: true });

  setTimeout(() => {
    t.assert.ok(true);
  }, 250);
});

test('should fail when assertions fail', async (t) => {
  t.plan(1, { wait: true });

  setTimeout(() => {
    t.assert.ok(false);
  }, 250).unref();
});
