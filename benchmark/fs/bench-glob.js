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
  pattern: ['**/*', '*.js', '**/**.js'],
  mode: ['sync', 'promise', 'callback'],
  recursive: ['true', 'false'],
};

const bench = common.createBenchmark(main, configs);

async function main(config) {
  const fullPath = path.resolve(benchmarkDirectory, config.dir);
  const { pattern, recursive, mode } = config;
  const options = { cwd: fullPath, recursive };
  const callback = (resolve, reject) => {
    glob(pattern, options, (err, matches) => {
      if (err) {
        reject(err);
      } else {
        resolve(matches);
      }
    });
  };

  let noDead;
  bench.start();

  for (let i = 0; i < config.n; i++) {
    switch (mode) {
      case 'sync':
        noDead = globSync(pattern, options);
        break;
      case 'promise':
        noDead = await globAsync(pattern, options);
        break;
      case 'callback':
        noDead = await new Promise(callback);
        break;
      default:
        throw new Error(`Unknown mode: ${mode}`);
    }
  }

  bench.end(config.n);
  assert.ok(noDead);
}
