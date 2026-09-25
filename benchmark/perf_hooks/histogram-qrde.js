'use strict';

const common = require('../common.js');
const { createHistogram } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [5],
  bins: [100, 1000],
  samples: [1e6],
  unique: [100, 1000, 10000],
  dequantize: ['none', 'hdr', 'all'],
}, {
  test: {
    n: 1,
    bins: 10,
    samples: 100,
    unique: 10,
  },
});

async function main({ n, bins, samples, unique, dequantize }) {
  const histogram = createHistogram();
  const maximum = 1e12;

  for (let i = 0; i < samples; i++) {
    const index = i % unique;
    const rank = unique === 1 ? 0 : index / (unique - 1);
    histogram.record(Math.max(1, Math.round(maximum ** rank)));
  }

  await histogram.qrde({ bins, dequantize });
  bench.start();
  for (let i = 0; i < n; i++) {
    await histogram.qrde({ bins, dequantize });
  }
  bench.end(n);
}
