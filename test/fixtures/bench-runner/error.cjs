'use strict';

const { bench } = require('node:bench');

bench('failure', {
  params: { kind: 'structured-error' },
  samples: 1,
}, () => {
  const error = new Error('benchmark fixture failed', {
    cause: { value: 42n },
  });
  error.code = 'ERR_BENCHMARK_FIXTURE';
  throw error;
});
