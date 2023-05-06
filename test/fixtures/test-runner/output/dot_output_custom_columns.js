// Flags: --test-reporter=dot
'use strict';
process.stdout.columns = 30;

require('../../../common');
const test = require('node:test');

for (let i = 0; i < 100; i++) {
  test(i + ' example', () => {});
}