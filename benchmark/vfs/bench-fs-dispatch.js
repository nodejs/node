'use strict';

// Measures the dispatch overhead the VFS layer adds to fs operations
// against real-filesystem paths (paths that no VFS claims). The hot
// path in lib/fs.js is:
//
//   const h = vfsState.handlers;
//   if (h !== null) { const r = h.opSync(path, ...); if (r !== undefined) return r; }
//   binding.op(getValidatedPath(path));
//
// With layers=0 the VFS module is never required and `h === null` is
// the first thing fs sees. With layers>=1 the handler walks
// activeVFSList calling vfs.shouldHandle(path) on each. The benchmark
// mounts N VFSes at distinct, unrelated mount points and probes a real
// file under __dirname, so every call falls through after the VFS list
// declines the path. That isolates per-layer dispatch cost.

const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [3e5],
  op: ['statSync', 'existsSync', 'accessSync', 'readFileSync'],
  // 0 = VFS module never loaded (true baseline)
  // >=1 = that many VFS instances mounted at unrelated paths
  layers: [0, 1, 2, 5, 10],
}, {
  flags: ['--experimental-vfs', '--no-warnings'],
});

function mountLayers(count) {
  const vfs = require('node:vfs');
  const handles = [];
  for (let i = 0; i < count; i++) {
    const v = vfs.create();
    v.mount(`/vfs-bench-${i}`);
    handles.push(v);
  }
  return handles;
}

function main({ n, op, layers }) {
  const handles = layers > 0 ? mountLayers(layers) : null;

  const target = layers === 0 ? __filename : path.join(__dirname, path.basename(__filename));

  // Warm-up - get the JIT past the first-call icache + IC misses so we
  // measure steady-state dispatch cost, not first-call resolution.
  for (let i = 0; i < 1000; i++) {
    if (op === 'statSync') fs.statSync(target);
    else if (op === 'existsSync') fs.existsSync(target);
    else if (op === 'accessSync') fs.accessSync(target);
    else fs.readFileSync(target);
  }

  bench.start();
  if (op === 'statSync') {
    for (let i = 0; i < n; i++) fs.statSync(target);
  } else if (op === 'existsSync') {
    for (let i = 0; i < n; i++) fs.existsSync(target);
  } else if (op === 'accessSync') {
    for (let i = 0; i < n; i++) fs.accessSync(target);
  } else {
    for (let i = 0; i < n; i++) fs.readFileSync(target);
  }
  bench.end(n);

  if (handles) {
    for (const v of handles) v.unmount();
  }
}
