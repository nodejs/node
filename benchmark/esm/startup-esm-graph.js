'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  modules: ['0250', '0500', '1000', '2000'],
  n: [30],
});

const BRANCHING_FACTOR = 10;

function prepare(count) {
  tmpdir.refresh();
  const dir = tmpdir.resolve('esm-graph');
  fs.mkdirSync(dir, { recursive: true });

  // Create a tree-shaped ESM graph with a branching factor of 10.
  // The root (mod0) plus `count` additional modules are created in BFS order.
  // Module i imports modules BRANCHING_FACTOR*i+1 through
  // BRANCHING_FACTOR*i+BRANCHING_FACTOR (capped at count), so the graph is a
  // complete 10-ary tree rooted at mod0.
  const total = count + 1;
  for (let i = 0; i < total; i++) {
    const children = [];
    for (let c = 1; c <= BRANCHING_FACTOR; c++) {
      const child = BRANCHING_FACTOR * i + c;
      if (child < total) {
        children.push(`import './mod${child}.mjs';`);
      }
    }
    const content = children.join('\n') + (children.length ? '\n' : '') +
      `export const value${i} = ${i};\n`;
    fs.writeFileSync(path.join(dir, `mod${i}.mjs`), content);
  }

  return path.join(dir, 'mod0.mjs');
}

function main({ n, modules }) {
  const entry = prepare(Number(modules));
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
