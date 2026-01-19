const { test } = require('node:test')

test('should fail on first two attempts', ({ attempt }) => {
  if (attempt < 2) {
    throw new Error('This test is expected to fail on the first two attempts');
  }
});

test('ok', ({ attempt }) => {
  if (attempt > 0) {
      throw new Error('Test should not rerun once it has passed');
  }
});


function ambiguousTest(expectedAttempts) {
  test(`ambiguous (expectedAttempts=${expectedAttempts})`, ({ attempt }) => {
    if (attempt < expectedAttempts) {
      throw new Error(`This test is expected to fail on the first ${expectedAttempts} attempts`);
    }
  });
}

ambiguousTest(0);
ambiguousTest(1);