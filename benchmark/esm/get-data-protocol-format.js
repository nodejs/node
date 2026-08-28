// Benchmarks defaultGetFormat() on `data:` URLs. The MIME-matching regex used
// to be susceptible to catastrophic backtracking on malformed input lacking a
// `,` separator (https://github.com/nodejs/node/issues/61904); `pathLength`
// scales the malformed path so a regression shows up as a sharp drop in ops/sec
// rather than a hang.
'use strict';

const common = require('../common.js');

const configs = {
  n: [1e4],
  pathLength: [1e2, 1e3, 1e4],
};

const options = {
  flags: ['--expose-internals'],
};

const bench = common.createBenchmark(main, configs, options);

function main({ n, pathLength }) {
  const { defaultGetFormat } = require('internal/modules/esm/get_format');
  const url = new URL(`data:a/${'a'.repeat(pathLength)}B`);

  bench.start();
  for (let i = 0; i < n; i++) {
    defaultGetFormat(url, { parentURL: undefined });
  }
  bench.end(n);
}
