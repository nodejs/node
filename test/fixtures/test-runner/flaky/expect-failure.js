'use strict';
const { test } = require('node:test');
const fs = require('node:fs');
const stateFile = process.env.FLAKY_STATE;
test('expected failure with flaky', { flaky: 5, expectFailure: true }, () => {
  fs.appendFileSync(stateFile, 'x');
  throw new Error('this failure is expected');
});
