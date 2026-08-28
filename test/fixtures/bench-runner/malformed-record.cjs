'use strict';

const common = require('../../common');

const record = process.env.NODE_BENCH_MALFORMED_RECORD === 'summary' ? {
  type: 'bench:summary',
  data: {
    counts: { completed: 0, failed: 0, skipped: 0, total: -1 },
    duration_ns: 1n,
    success: true,
  },
} : null;

process.send?.({ type: 'node:bench:record', record });
setTimeout(() => process.exit(2), common.platformTimeout(10_000));
