// Flags: --no-warnings --test-name-pattern="first parent > enabled > third level"
'use strict';
const common = require('../../../common');
const { test, describe } = require('node:test');

console.log(process.execArgv);
describe('first parent', common.mustCall(() => {
  test('enabled', common.mustCall(() => {
    test('third level', common.mustCall());
    test('disabled', common.mustNotCall());
  }));
  test('disabled', common.mustNotCall());
}));

test('second parent', common.mustNotCall());
