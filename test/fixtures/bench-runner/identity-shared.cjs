'use strict';

const { bench } = require('node:bench');

module.exports = function registerSharedIdentity() {
  bench('shared identity', { samples: 1 }, (b) => {
    b.start();
    process.hrtime.bigint();
    b.end(1);
  });
};
