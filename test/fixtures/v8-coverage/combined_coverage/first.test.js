'use strict';
const { test } = require('node:test');
const common = require('./common');

function notCalled() {
}

test('first 1', () => {
  common.fnA();
  common.fnD(100);
  common.fnB([1, 2, 3]);
});
