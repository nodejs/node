'use strict';

// Measures VFS dispatch overhead on fs ops against real-filesystem paths
// (paths no VFS claims). Layers=0 means the VFS module is never loaded.

const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [3e5],
  op: ['statSync', 'existsSync', 'accessSync', 'readFileSync'],
  layers: [0, 1, 2, 5, 10],
}, {
  flags: ['--experimental-vfs', '--no-warnings'],
});

function mountLayers(count) {
  const vfs = require('node:vfs');
  const handles = [];
  for (let i = 0; i < count; i++) {
    const v = vfs.create();
    v.mount();
    handles.push(v);
  }
  return handles;
}

function main({ n, op, layers }) {
  const handles = layers > 0 ? mountLayers(layers) : null;

  const target = layers === 0 ? __filename : path.join(__dirname, path.basename(__filename));

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
