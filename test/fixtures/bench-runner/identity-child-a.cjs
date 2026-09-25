'use strict';

const { completeSample } = require('../../common/bench');
const { bench } = require('node:bench');

module.exports = function declareChildA() {
  bench('child a', { samples: 1 }, (b) => {
    completeSample(b);
  });
};
