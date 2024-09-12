'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const benchmarkDirectory = path.resolve(__dirname, '..', '..');

const configs = {
  n: [1, 10, 100],
  dir: ['lib', 'test/parallel', 'benchmark'],
  pattern: ['**/*', '*.js', '**/**.js'],
  mode: ['async', 'sync'],
  recursive: ['true', 'false'],
};

const bench = common.createBenchmark(main, configs);

async function main(config) {
  const fullPath = path.resolve(benchmarkDirectory, config.dir);
  const { pattern, recursive, mode } = config;

  bench.start();

  let counter = 0;

  for (let i = 0; i < config.n; i++) {
    if (mode === 'async') {
      const matches = await new Promise((resolve, reject) => {
        fs.glob(pattern, { cwd: fullPath, recursive }, (err, files) => {
          if (err) {
            reject(err);
          } else {
            resolve(files);
          };
        });
      });

      counter += matches.length;
    } else {
      const matches = fs.globSync(pattern, { cwd: fullPath, recursive });
      counter += matches.length;
    }
  }

  bench.end(counter);
}
