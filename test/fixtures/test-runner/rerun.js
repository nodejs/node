const { test, describe } = require('node:test')

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

function nestedAmbiguousTest(expectedAttempts) {
  return async (t) => {
    await t.test('nested', async (tt) => {
      await tt.test('2 levels deep', () => {});
      if (t.attempt < expectedAttempts) {
        throw new Error(`This test is expected to fail on the first ${expectedAttempts} attempts`);
      }
    });
    await t.test('ok', () => {});
  };
}

test('nested ambiguous (expectedAttempts=0)', nestedAmbiguousTest(0));
test('nested ambiguous (expectedAttempts=1)', nestedAmbiguousTest(2));


describe('describe rerun', { timeout: 1000, concurrency: 1000 }, () => {
  test('passed on first attempt', async (t) => {
    await t.test('nested', async () => {});
  });
  test('a');
});
