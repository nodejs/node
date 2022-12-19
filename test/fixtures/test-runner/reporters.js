'use strict';
const test = require('node:test');

test('nested', { concurrency: 4 }, async (t) => {
  t.test('ok', () => {});
  t.test('failing', () => {
    throw new Error('error');
  });
});

test('top level', () => {});
