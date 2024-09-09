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
  recursive: ['true', 'false']
};

const bench = common.createBenchmark(main, configs);

async function main(conf) {
  const fullPath = path.resolve(benchmarkDirectory, conf.dir);
  const globPattern = conf.pattern;
  const recursive = conf.recursive === 'true';

  bench.start();

  let counter = 0;
  for (let i = 0; i < conf.n; i++) {
    if (conf.mode === 'async') {
      const matches = await new Promise((resolve, reject) => {
        fs.glob(globPattern, { cwd: fullPath, recursive }, (err, files) => {
          if (err) reject(err);
          else resolve(files);
        });
      });
      counter += matches.length;
    } else if (conf.mode === 'sync') {
      const matches = fs.globSync(globPattern, { cwd: fullPath, recursive });
      counter += matches.length;
    }
  }

  bench.end(counter);
}
