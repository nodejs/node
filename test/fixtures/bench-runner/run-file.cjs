'use strict';

const { bench } = require('node:bench');

bench('run file', {
  samples: 1,
  params: {
    context: process.env.NODE_BENCH_CONTEXT,
    exposed: typeof globalThis.gc === 'function',
    value: process.env.NODE_BENCH_RUN_FILE ?? 'unset',
  },
}, (b) => {
  b.record({
    duration_ns: 1n,
    operations: 1,
    detail: {
      execArgv: process.execArgv,
      pid: process.pid,
    },
  });
});
