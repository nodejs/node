'use strict';

const common = require('../common');
const {
  glob,
  globSync,
  promises: { glob: globAsync },
} = require('fs');
const path = require('path');
const assert = require('node:assert');

const benchmarkDirectory = path.resolve(__dirname, '..', '..');

const configs = {
  n: [1e3],
  dir: ['lib'],
  pattern: ['**/*', '*.js', '**/*.js', '**/*.{js,json}'],
  mode: ['sync', 'promise', 'callback'],
  maxDepth: ['default', '2'],
};

const bench = common.createBenchmark(main, configs);

async function main(config) {
  const { pattern, mode, n, maxDepth } = config;
  const options = { cwd: path.resolve(benchmarkDirectory, config.dir) };
  if (maxDepth !== 'default') {
    options.maxDepth = Number(maxDepth);
  }

  let noDead;
  bench.start();

  for (let i = 0; i < n; i++) {
    switch (mode) {
      case 'sync':
        noDead = globSync(pattern, options);
        break;
      case 'promise':
        noDead = await Array.fromAsync(globAsync(pattern, options));
        break;
      case 'callback':
        noDead = await new Promise((resolve, reject) => {
          glob(pattern, options, (err, matches) => {
            if (err) {
              reject(err);
            } else {
              resolve(matches);
            }
          });
        });
        break;
      default:
        throw new Error(`Unknown mode: ${mode}`);
    }
  }

  bench.end(n);
  assert.ok(noDead.length > 0);
}
