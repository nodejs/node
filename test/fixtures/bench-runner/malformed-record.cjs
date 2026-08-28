'use strict';

const common = require('../../common');

const kind = process.env.NODE_BENCH_MALFORMED_RECORD;
const id = kind === 'sequence' ? null : 0;
const benchmarkData = {
  __proto__: null,
  benchId: 'invalid plan',
  column: 1,
  entryFile: __filename,
  file: __filename,
  fileRunId: process.env.NODE_BENCH_FILE_RUN_ID,
  line: 1,
  name: 'invalid plan',
  namePath: ['invalid plan'],
  params: {},
  parentId: null,
  runId: process.env.NODE_BENCH_RUN_ID,
  tags: [],
};
const plan = {
  type: 'bench:plan',
  data: {
    __proto__: null,
    ...benchmarkData,
    samples: 1,
    selected: true,
    timeout: null,
    warmup: 0,
    yieldBetweenSamples: true,
  },
};
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
} : kind === 'plan' ? {
  ...plan,
  data: {
    ...plan.data,
    samples: 0,
  },
} : kind === 'identity' ? {
  type: 'bench:complete',
  data: {
    entryFile: __filename,
    fileRunId: process.env.NODE_BENCH_FILE_RUN_ID,
    runId: process.env.NODE_BENCH_RUN_ID,
  },
} : null;

process.send?.({ id, type: 'node:bench:record', record });
setTimeout(() => process.exit(2), common.platformTimeout(10_000));
