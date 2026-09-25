'use strict';
// A flaky + expectFailure test whose body does NOT throw is an unexpected pass
// (xpass), which is a verdict failure - so it IS retried like any other flaky
// failure. It never reaches the expected failure, so with flaky: 5 it exhausts
// the retry budget and is reported as a failure with retryCount === 5.
const { test } = require('node:test');

test('flaky expectFailure unexpected pass', { flaky: 5, expectFailure: true }, () => {
  // Body intentionally does not throw: every attempt is an unexpected pass.
});
