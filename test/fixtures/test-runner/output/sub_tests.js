'use strict';
require('../../../common');
const { test } = require('node:test');

test('parent 1', async (t) => {
  await t.test('running subtest 1');
  await t.test.skip('skipped subtest 2');
  await t.test.todo('mark subtest 3 as todo');
});
