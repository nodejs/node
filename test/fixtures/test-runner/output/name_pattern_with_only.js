// Flags: --test-only --test-name-pattern=enabled
'use strict';
const common = require('../../../common');
const { test } = require('node:test');

test('enabled and only', { only: true }, common.mustCall(async (t) => {
  await t.test('enabled', common.mustCall());
  await t.test('disabled but parent not', common.mustCall());
}));

test('enabled but not only', common.mustNotCall());
test('only does not match pattern', { only: true }, common.mustNotCall());
test('not only and does not match pattern', common.mustNotCall());
