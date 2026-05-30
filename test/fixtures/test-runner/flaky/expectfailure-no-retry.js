'use strict';
// A flaky + expectFailure test whose body does NOT throw (an unexpected pass)
// must NOT be retried: expectFailure tests are not retried. The body appends one
// byte per run; the harness then asserts it ran exactly once.
const { test } = require('node:test');
const { appendFileSync } = require('node:fs');

const stateFile = process.env.FLAKY_STATE;

test('flaky expectFailure unexpected pass', { flaky: 5, expectFailure: true }, () => {
  appendFileSync(stateFile, 'x');
});
