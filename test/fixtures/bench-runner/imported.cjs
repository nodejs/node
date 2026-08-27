'use strict';

const { bench } = require('node:bench');

bench('imported', {
  samples: 1,
  params: { imported: globalThis.benchImport },
}, (b) => {
  b.start();
  process.hrtime.bigint();
  b.end(1);
});
