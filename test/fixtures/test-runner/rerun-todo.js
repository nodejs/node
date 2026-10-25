const { test, describe } = require('node:test');

test('top-level todo fails on first attempt', { todo: true }, ({ attempt }) => {
  if (attempt < 1) {
    throw new Error('This todo test is expected to fail on the first attempt');
  }
});

describe('suite with failing todo', () => {
  test('nested todo fails on first attempt', { todo: true }, ({ attempt }) => {
    if (attempt < 1) {
      throw new Error('This todo test is expected to fail on the first attempt');
    }
  });

  test('ok', ({ attempt }) => {
    if (attempt > 0) {
      throw new Error('Test should not rerun once it has passed');
    }
  });
});
