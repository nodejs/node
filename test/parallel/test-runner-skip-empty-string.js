'use strict';
const { test } = require('node:test');
const assert = require('assert');

let ran = false;

test('skip option empty string should not skip', { skip: '' }, () => {
  ran = true;
});

process.on('exit', () => {
  assert.ok(ran);
});