'use strict';
const fs = require('fs');
const path = require('path');
const common = require('../common.js');

// Measures what the fs hooks cost a call once a VFS is mounted: a real path
// only has to be told apart from the VFS root, while a path in a layer is
// resolved to the layer, either by its id or through its mount name.
const bench = common.createBenchmark(main, {
  target: ['real', 'id', 'name'],
  n: [1e5],
}, { flags: ['--experimental-vfs', '--no-warnings'] });

function main({ n, target }) {
  const vfs = require('node:vfs');
  const layer = vfs.create();
  layer.mkdirSync('/dir');
  layer.writeFileSync('/dir/file.txt', 'x');
  const mountPoint = layer.mount('bench');
  const file = {
    real: __filename,
    id: path.join(mountPoint, 'dir', 'file.txt'),
    name: path.join(path.dirname(mountPoint), 'bench', 'dir', 'file.txt'),
  }[target];

  bench.start();
  for (let i = 0; i < n; i++) {
    fs.statSync(file);
  }
  bench.end(n);
  layer.unmount();
}
