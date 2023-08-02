const test = require('node:test');

test('nested', (t) => {
  t.test('first', () => {});
  t.test('second', () => {
    throw new Error();
  });
  t.test('third', () => {});
});

test('top level', (t) => {
  t.test('forth', () => {});
  t.test('fifth', () => {
    throw new Error();
  });
});

