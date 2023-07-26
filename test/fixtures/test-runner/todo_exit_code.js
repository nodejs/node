const { describe, test } = require('node:test');

describe('suite should pass', () => {
  test.todo('should fail without harming suite', () => {
    throw new Error('Fail but not badly')
  });
});

test.todo('should fail without effecting exit code', () => {
  throw new Error('Fail but not badly')
});

test('empty string todo', { todo: '' }, () => {
  throw new Error('Fail but not badly')
});
