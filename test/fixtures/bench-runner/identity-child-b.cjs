'use strict';

const { bench } = require('node:bench');

module.exports = function declareChildB() {
  bench('child b', { samples: 1 }, (b) => {
    b.start();
    process.hrtime.bigint();
    b.end(1);
  });
};
