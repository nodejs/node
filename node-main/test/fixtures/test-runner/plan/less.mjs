import test from 'node:test';

test('less assertions than planned', (t) => {
  t.plan(2);
  t.assert.ok(true, 'only one assertion');
  // Missing second assertion
});
