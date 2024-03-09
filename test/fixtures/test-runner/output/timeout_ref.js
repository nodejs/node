'use strict';
const { test } = require('node:test');

test('callback test that times out', { timeout: 1 }, (t, done) => {});
test('promise test that times out', { timeout: 1 }, () => {
  return new Promise(() => {});
});
