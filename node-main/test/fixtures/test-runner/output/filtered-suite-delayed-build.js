// Flags: --test-name-pattern=enabled
'use strict';
const common = require('../../../common');
const { suite, test } = require('node:test');

suite('async suite', common.mustCall(async () => {
  await 1;
  test('enabled 1', common.mustCall());
  await 1;
  test('not run', common.mustNotCall());
  await 1;
}));

suite('sync suite', common.mustCall(() => {
  test('enabled 2', common.mustCall());
}));
