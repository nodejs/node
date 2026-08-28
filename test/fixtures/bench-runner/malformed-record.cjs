'use strict';

const common = require('../../common');

const kind = process.env.NODE_BENCH_MALFORMED_RECORD;
const record = kind === 'summary' ? {
  type: 'bench:summary',
  data: {
    counts: { completed: 0, failed: 0, skipped: 0, total: -1 },
    duration_ns: 1n,
    entryFile: __filename,
    fileRunId: process.env.NODE_BENCH_FILE_RUN_ID,
    runId: process.env.NODE_BENCH_RUN_ID,
    success: true,
  },
} : kind === 'identity' ? {
  type: 'bench:complete',
  data: {
    entryFile: __filename,
    fileRunId: process.env.NODE_BENCH_FILE_RUN_ID,
    runId: process.env.NODE_BENCH_RUN_ID,
  },
} : null;

process.send?.({ type: 'node:bench:record', record });
setTimeout(() => process.exit(2), common.platformTimeout(10_000));
