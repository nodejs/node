'use strict';

const { bench } = require('node:bench');

module.exports = function declareChildA() {
  bench('child a', { samples: 1 }, (b) => {
    b.start();
    process.hrtime.bigint();
    b.end(1);
  });
};
