'use strict';
const { test } = require('node:test');

test('test 3 passes', () => {
  // This should not run if bail happens in test 2
});

test('test 4 passes', () => {
  // This should not run if bail happens in test 2
});
