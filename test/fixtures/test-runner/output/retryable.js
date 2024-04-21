const common = require('../../../common');
const test = require('node:test');
const assert = require('node:assert');

test.describe('hardcoded retries', () => {
  test('should retry 3 times (success)', {
    retries: 5,
  }, common.mustCallAtLeast((t) => {
    if (t.currentAttempt < 3) {
      throw new Error('fail');
    }
  }, 3));

  // Expect: Failure
  test('should retry 5 times (failure)', {
    retries: 5,
  }, common.mustCallAtLeast(() => {
    throw new Error('fail');
  }, 5));
});

test.describe('dynamic retries', () => {
  test('should retry 3 times (success)', common.mustCallAtLeast((t) => {
    t.retries(5);
    if (t.currentAttempt < 3) {
      throw new Error('fail');
    }
  }, 3));

  test('should retry 3 times (failure)', common.mustCallAtLeast((t) => {
    t.retries(3);
    throw new Error('fail');
  }, 3));
});

test.describe('multiple retries', () => {
  test('should retry 4 times (success)', {
    retries: 3,
  }, common.mustCallAtLeast((t) => {
    t.retries(5);
    if (t.currentAttempt < 4) {
      throw new Error('fail');
    }
  }, 4));
});

test.describe('hooks', async () => {
  test.beforeEach(common.mustCallAtLeast(() => {}, 4));
  test.afterEach(common.mustCallAtLeast(() => {}, 4));
  test.before(common.mustCallAtLeast(() => {}, 4));
  test.after(common.mustCallAtLeast(() => {}, 4));

  test('should call before/after hooks (success)', {
    retries: 3,
  }, (t) => {
    if (t.attempts < 3) {
      throw new Error('fail');
    }
  });
})
