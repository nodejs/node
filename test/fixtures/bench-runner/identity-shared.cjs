'use strict';

const { completeSample } = require('../../common/bench');
const { bench } = require('node:bench');

module.exports = function registerSharedIdentity() {
  bench('shared identity', { samples: 1 }, (b) => {
    completeSample(b);
  });
};
