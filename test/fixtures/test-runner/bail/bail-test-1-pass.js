'use strict';
const { test } = require('node:test');
const { setTimeout } = require('timers/promises');

test('test 1 passes', async () => {
  // This should pass
  await setTimeout(500);
});

test('test 2 passes', () => {
  // This should pass
});
