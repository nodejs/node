'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const assert = require('node:assert');

const benchmarkDirectory = path.resolve(__dirname, '..', '..');

const configs = {
  n: [1e3],
  dir: ['lib', 'test/parallel', 'benchmark'],
  pattern: ['**/*', '*.js', '**/**.js'],
  useAsync: [true, false],
  recursive: [true, false],
};

const bench = common.createBenchmark(main, configs);

async function main(config) {
  const fullPath = path.resolve(benchmarkDirectory, config.dir);
  const { pattern, recursive, useAsync } = config;

  let noDead;
  bench.start();

  for (let i = 0; i < config.n; i++) {
    if (useAsync) {
      noDead = await new Promise((resolve, reject) => {
        fs.glob(pattern, { cwd: fullPath, recursive }, (err, files) => {
          if (err) {
            reject(err);
          } else {
            resolve(files);
          };
        });
      });
    } else {
      noDead = fs.globSync(pattern, { cwd: fullPath, recursive });
    }
  }

  bench.end(config.n);
  assert.ok(noDead);
}
