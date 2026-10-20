'use strict';

const { completeSample } = require('../../common/bench');
const { bench } = require('node:bench');

bench('declared before load error', { samples: 1 }, (b) => {
  completeSample(b);
});

throw new Error('load failed after declaration');
