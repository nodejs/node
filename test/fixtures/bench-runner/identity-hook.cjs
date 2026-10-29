'use strict';

const { before, bench } = require('node:bench');

before(() => {
  throw new Error('scoped hook failed');
});

bench('scoped hook benchmark', { samples: 1 }, () => {});
