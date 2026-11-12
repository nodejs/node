'use strict';

const common = require('../common.js');
const fs = require('fs');
const path = require('path');
const { pathToFileURL } = require('url');
const tmpdir = require('../../test/common/tmpdir');

const BRANCHING_FACTOR = 10;
const benchmarkDirectory = tmpdir.resolve('esm-graph');

// Each measured iteration has to load a graph the ESM cache has never seen, so the
// fixture holds one independent copy of the graph per iteration.
function graphDirectory(modules, copy) {
  return path.join(benchmarkDirectory, `g${modules}-${copy}`);
}

function entryURL(modules, copy) {
  return pathToFileURL(path.join(graphDirectory(modules, copy), 'mod0.mjs')).href;
}

// Build a complete BRANCHING_FACTOR-ary tree of `modules` + 1 modules rooted at mod0:
// module i imports modules BRANCHING_FACTOR*i+1 through BRANCHING_FACTOR*i+BRANCHING_FACTOR,
// capped at the total, so the shape approximates a real dependency tree rather than
// one module with hundreds of direct imports.
function createGraph(modules, copy) {
  const dir = graphDirectory(modules, copy);
  fs.mkdirSync(dir, { recursive: true });
  const total = modules + 1;
  for (let i = 0; i < total; i++) {
    let source = '';
    for (let c = 1; c <= BRANCHING_FACTOR; c++) {
      const child = BRANCHING_FACTOR * i + c;
      if (child < total) {
        source += `import './mod${child}.mjs';\n`;
      }
    }
    source += `export const value${i} = ${i};\n`;
    fs.writeFileSync(path.join(dir, `mod${i}.mjs`), source);
  }
}

const bench = common.createBenchmark(main, {
  modules: [250, 500, 1000, 2000],
  n: [10],
}, {
  setup(configs) {
    // Build every fixture once here rather than per configuration: writing tens of
    // thousands of files is far more expensive than the work being measured.
    tmpdir.refresh();
    const maxN = configs.reduce((max, c) => Math.max(max, c.n), 0);
    for (const modules of new Set(configs.map((c) => c.modules))) {
      for (let copy = 0; copy < maxN; copy++) {
        createGraph(modules, copy);
      }
    }
  },
});

async function main({ n, modules }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    await import(entryURL(modules, i));
  }
  bench.end(n);
}
