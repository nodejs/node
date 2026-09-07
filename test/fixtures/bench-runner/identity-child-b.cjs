'use strict';

const { completeSample } = require('../../common/bench');
const { bench } = require('node:bench');

module.exports = function declareChildB() {
  bench('child b', { samples: 1 }, (b) => {
    completeSample(b);
  });
};
