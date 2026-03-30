'use strict';
const { test } = require('node:test');
const common = require('./common');

test('second 1', () => {
  common.fnB([1, 2, 3]);
  common.fnD(3);
});
