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


// Shared helper creates subtests at the same source location each time it's
// called, producing ambiguous test identifiers (disambiguated with ":(N)"
// suffixes in the rerun state file). Regression coverage for a bug where the
// suite's synthetic rerun fn double-started its direct children, which then
// re-invoked the synthetic descendant-creator against an already-incremented
// disambiguator map and emitted spurious failures.
function ambiguousHelper(t) {
  return Promise.all([
    t.test('shared sub A', () => {}),
    t.test('shared sub B', () => {}),
  ]);
}

describe('rerun with ambiguous shared helper', { timeout: 1000, concurrency: 1000 }, () => {
  test('first caller', (t) => ambiguousHelper(t));
  test('second caller', (t) => ambiguousHelper(t));
});
