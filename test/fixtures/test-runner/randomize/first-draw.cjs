'use strict';

const test = require('node:test');

test('parent', (t) => {
  t.test('a', () => {});
  t.test('b', () => {});
  t.test('c', () => {});
});
