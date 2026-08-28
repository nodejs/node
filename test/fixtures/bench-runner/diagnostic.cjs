'use strict';

const { bench } = require('node:bench');

bench('diagnostic relay', { samples: 1 }, (b) => {
  b.diagnostic('relayed warning', {
    detail: { value: 42n },
    level: 'warning',
  });
  b.record({ duration_ns: 1n, operations: 1 });
});
