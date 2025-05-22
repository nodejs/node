'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('node:assert');

const benchmarkDirectory = path.resolve(__dirname, '..', '..');

const configs = {
  n: [1e3],
  dir: ['lib'],
  pattern: ['**/*', '*.js', '**/**.js'],
  mode: ['sync', 'promise', 'callback'],
  recursive: ['true', 'false'],
};

const bench = common.createBenchmark(main, configs);

async function main(config) {
  const fullPath = path.resolve(benchmarkDirectory, config.dir);
  const { pattern, recursive, mode } = config;

  let noDead;
  bench.start();

  for (let i = 0; i < config.n; i++) {
    switch (mode) {
      case 'sync':
        noDead = fs.globSync(pattern, { cwd: fullPath, recursive });
        break;
      case 'promise':
        noDead = await fs.promises.glob(pattern, { cwd: fullPath, recursive });
        break;
      case 'callback':
        noDead = await new Promise((resolve, reject) => {
          fs.glob(pattern, { cwd: fullPath, recursive }, (err, matches) => {
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

  bench.end(config.n);
  assert.ok(noDead);
}
