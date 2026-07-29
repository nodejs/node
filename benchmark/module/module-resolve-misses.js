'use strict';

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const common = require('../common.js');

const tmpdir = require('../../test/common/tmpdir');
const benchmarkDirectory = tmpdir.resolve('nodejs-benchmark-module');

const bench = common.createBenchmark(main, {
  depth: [4, 12],
  deps: [200],
  n: [30],
});

function main({ depth, deps, n }) {
  tmpdir.refresh();

  // The only `node_modules` that exists: at the root, above the requirer.
  const nodeModules = path.join(benchmarkDirectory, 'node_modules');
  for (let i = 0; i < deps; i++) {
    const dep = path.join(nodeModules, `dep${i}`);
    fs.mkdirSync(dep, { recursive: true });
    fs.writeFileSync(
      path.join(dep, 'package.json'),
      `{"name":"dep${i}","main":"index.js"}`,
    );
    fs.writeFileSync(path.join(dep, 'index.js'), 'module.exports = {};');
  }

  // The requirer, nested `depth` levels down. Resolution walks up from here and
  // finds no `node_modules` until the root.
  const nested = path.join(benchmarkDirectory, ...'x'.repeat(depth).split(''));
  fs.mkdirSync(nested, { recursive: true });

  let entrySource = '';
  for (let i = 0; i < deps; i++) {
    entrySource += `require('dep${i}');\n`;
  }
  const entry = path.join(nested, 'entry.js');
  fs.writeFileSync(entry, entrySource);

  const cmd = process.execPath || process.argv[0];
  const warmup = 3;
  for (let i = -warmup; i < n; i++) {
    if (i === 0) {
      bench.start();
    }
    const child = spawnSync(cmd, [entry]);
    if (child.status !== 0) {
      throw new Error(`Child process stopped with exit code ${child.status}`);
    }
  }
  bench.end(n);

  tmpdir.refresh();
}
