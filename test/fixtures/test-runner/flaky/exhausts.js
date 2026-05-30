'use strict';
const { test } = require('node:test');
test('always fails', { flaky: 3 }, () => {
  throw new Error('boom');
});
