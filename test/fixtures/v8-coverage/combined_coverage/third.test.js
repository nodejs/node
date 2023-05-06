'use strict';
const { test } = require('node:test');
const common = require('./common');

test('third 1', () => {
  common.fnC(1, 4);
  common.fnD(99);
});
