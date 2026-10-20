// Flags: --test --test-reporter=junit
'use strict';
const test = require('node:test');

test('failing', (t) => {
  t.diagnostic('');
  throw new Error('error');
});
