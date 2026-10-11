'use strict';

const { bench } = require('node:bench');

const options = { samples: 3, warmup: 2 };
if (process.env.BENCH_GC_OPTION) options.gc = process.env.BENCH_GC_OPTION;

bench('gc', options, (b) => {
  b.record({ detail: typeof globalThis.gc, duration_ns: 1n });
});
