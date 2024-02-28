'use strict';
// Refs: https://github.com/nodejs/node/issues/51371
const common = require('../common');
const { test } = require('node:test');

test('test', async (t) => {
  t.after(common.mustNotCall(() => {
    t.fail('should not run');
  }));
});
