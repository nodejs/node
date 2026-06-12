'use strict';

// Startup benchmark for the compile cache (including the zstd dictionary).
// Compares no-cache / cold-cache / warm-cache for two workloads:
//   big  - one large module (the typescript.js fixture)
//   many - many small modules (generated here, side-effect-free)
// The modules are generated into a temp dir so the benchmark is self-contained
// and reproducible, and never executes unrelated code.

const common = require('../common.js');
const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

const bench = common.createBenchmark(main, {
  workload: ['big', 'many'],
  cache: ['none', 'cold', 'warm'],
  n: [30],
});

const BIG = path.resolve(__dirname, '../../test/fixtures/snapshot/typescript.js');

// Generate `count` small, side-effect-free modules and return the require()
// code that loads them all in one child.
function makeManyModules(dir, count) {
  fs.mkdirSync(dir, { recursive: true });
  const reqs = [];
  for (let i = 0; i < count; i++) {
    const file = path.join(dir, `mod-${i}.js`);
    fs.writeFileSync(
      file,
      `'use strict';\n` +
      `module.exports = function value${i}(a, b) {\n` +
      `  const sum = a + b + ${i};\n` +
      `  return { id: ${i}, sum, label: 'module-${i}' };\n` +
      `};\n`);
    reqs.push(`require(${JSON.stringify(file)});`);
  }
  return reqs.join('');
}

function run(cmd, args, cacheDir) {
  const env = { ...process.env };
  if (cacheDir) env.NODE_COMPILE_CACHE = cacheDir;
  else delete env.NODE_COMPILE_CACHE;
  const child = spawnSync(cmd, args, { env, stdio: 'ignore' });
  if (child.error) throw child.error;
}

function main({ n, workload, cache }) {
  const cmd = process.execPath || process.argv[0];
  const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'cc-bench-'));
  const args = workload === 'big' ?
    [BIG] :
    ['-e', makeManyModules(path.join(tmp, 'mods'), 120)];
  const cacheDir = cache === 'none' ? null : path.join(tmp, 'cache');

  try {
    if (cache === 'warm') run(cmd, args, cacheDir);  // populate once
    bench.start();
    for (let i = 0; i < n; i++) {
      if (cache === 'cold' && cacheDir) {
        fs.rmSync(cacheDir, { recursive: true, force: true });
      }
      run(cmd, args, cacheDir);
    }
    bench.end(n);
  } finally {
    fs.rmSync(tmp, { recursive: true, force: true });
  }
}
