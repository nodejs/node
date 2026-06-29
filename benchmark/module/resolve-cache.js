'use strict';

// Measures the cold-start cost of resolving a dependency tree with and without
// the persistent module-resolution cache (NODE_MODULE_RESOLVE_CACHE). The
// cache benefits warm starts of fresh processes, so this benchmark spawns a
// child process per iteration.

const common = require('../common.js');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  files: [1000],
  cache: ['true', 'false'],
  n: [30],
});

function generate(dir, files) {
  fs.mkdirSync(path.join(dir, 'node_modules'), { recursive: true });
  let app = '';
  for (let i = 0; i < files; i++) {
    const pkg = path.join(dir, 'node_modules', `pkg${i}`);
    fs.mkdirSync(pkg);
    fs.writeFileSync(path.join(pkg, 'package.json'),
                     `{"name":"pkg${i}","version":"1.0.0","main":"index.js"}`);
    fs.writeFileSync(path.join(pkg, 'index.js'), `module.exports=${i};`);
    app += `require("pkg${i}");`;
  }
  fs.writeFileSync(path.join(dir, 'package-lock.json'), '{}');
  const appFile = path.join(dir, 'app.js');
  fs.writeFileSync(appFile, app);
  return appFile;
}

function main({ n, files, cache }) {
  tmpdir.refresh();
  const dir = tmpdir.resolve('resolve-cache-bench');
  const appFile = generate(dir, files);
  const cmd = process.execPath || process.argv[0];

  const env = { ...process.env };
  if (cache === 'true') {
    env.NODE_MODULE_RESOLVE_CACHE = tmpdir.resolve('resolve-cache-dir');
    // Warm the cache once.
    spawnSync(cmd, [appFile], { cwd: dir, env });
  } else {
    delete env.NODE_MODULE_RESOLVE_CACHE;
  }

  // Warmup.
  for (let i = 0; i < 3; i++) spawnSync(cmd, [appFile], { cwd: dir, env });

  bench.start();
  for (let i = 0; i < n; i++) {
    const child = spawnSync(cmd, [appFile], { cwd: dir, env });
    if (child.status !== 0) {
      throw new Error(`Child exited with ${child.status}: ${child.stderr}`);
    }
  }
  bench.end(n);

  tmpdir.refresh();
}
