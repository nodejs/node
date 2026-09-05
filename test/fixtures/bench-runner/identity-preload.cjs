'use strict';

const { completeSample } = require('../../common/bench');
const { bench } = require('node:bench');

bench('preload identity', { samples: 1 }, (b) => {
  completeSample(b);
});
