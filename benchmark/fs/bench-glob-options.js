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
  pattern: ['**/*.js'],
  mode: ['sync', 'promise', 'callback'],
  options: ['none', 'withFileTypes', 'exclude-pattern', 'exclude-callback'],
};

const bench = common.createBenchmark(main, configs);

function buildOptions(config) {
  const options = { cwd: path.resolve(benchmarkDirectory, config.dir) };
  switch (config.options) {
    case 'none':
      break;
    case 'withFileTypes':
      options.withFileTypes = true;
      break;
    case 'exclude-pattern':
      options.exclude = ['**/internal/**'];
      break;
    case 'exclude-callback':
      // Excludes nothing: measures the per-entry callback overhead alone.
      options.exclude = () => false;
      break;
    default:
      throw new Error(`Unknown options: ${config.options}`);
  }
  return options;
}

async function main(config) {
  const { pattern, mode, n } = config;
  const options = buildOptions(config);

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
