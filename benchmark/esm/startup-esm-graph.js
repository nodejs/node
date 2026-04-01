'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  modules: [250, 500, 1000, 2000],
  n: [30],
});

function prepare(count) {
  tmpdir.refresh();
  const dir = tmpdir.resolve('esm-graph');
  fs.mkdirSync(dir, { recursive: true });

  // Create a flat ESM graph: entry imports all modules directly.
  // Each module is independent, maximizing the number of resolve/load/link
  // operations in the loader pipeline.
  const imports = [];
  for (let i = 0; i < count; i++) {
    fs.writeFileSync(
      path.join(dir, `mod${i}.mjs`),
      `export const value${i} = ${i};\n`,
    );
    imports.push(`import './mod${i}.mjs';`);
  }

  const entry = path.join(dir, 'entry.mjs');
  fs.writeFileSync(entry, imports.join('\n') + '\n');
  return entry;
}

function main({ n, modules }) {
  const entry = prepare(modules);
  const cmd = process.execPath || process.argv[0];
  const warmup = 3;
  const state = { finished: -warmup };

  while (state.finished < n) {
    const child = spawnSync(cmd, [entry]);
    if (child.status !== 0) {
      console.log('---- STDOUT ----');
      console.log(child.stdout.toString());
      console.log('---- STDERR ----');
      console.log(child.stderr.toString());
      throw new Error(`Child process stopped with exit code ${child.status}`);
    }
    state.finished++;
    if (state.finished === 0) {
      bench.start();
    }
    if (state.finished === n) {
      bench.end(n);
    }
  }
}
