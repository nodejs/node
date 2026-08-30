'use strict';

const { bench } = require('node:bench');

bench('imported', {
  samples: 1,
  params: { imported: globalThis.benchImport },
}, (b) => {
  b.record({ duration_ns: 1n, operations: 1 });
});
