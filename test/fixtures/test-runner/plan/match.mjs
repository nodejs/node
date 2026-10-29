import test from 'node:test';

test('matching assertions', (t) => {
  t.plan(2);
  t.assert.ok(true, 'first assertion');
  t.assert.ok(true, 'second assertion');
});
