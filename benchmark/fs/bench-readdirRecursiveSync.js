'use strict';

const common = require('../common.js');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../../test/common/tmpdir');

const bench = common.createBenchmark(main, {
  n: [1, 500], // Number of times to read directory
  depth: [2, 16], // Directory depth
  files: [10, 150], // Files per directory
  withFileTypes: ['true', 'false'], // Whether to include file types
});

function makeTree({ depth, files, current = 0 }) {
  if (current >= depth) return;

  const currentDir = path.join(tmpdir.path, `d${current}`);
  fs.mkdirSync(currentDir, { recursive: true });

  for (let i = 0; i < files; i++) {
    fs.writeFileSync(path.join(tmpdir.path, `d${current}`, `f${i}`), 'content');
  }

  const subdir = path.join(tmpdir.path, `d${current}`, `d${current + 1}`);
  fs.mkdirSync(subdir, { recursive: true });
  makeTree({ depth, files, current: current + 1 });
}

function main({ n, depth, files, withFileTypes }) {
  withFileTypes = withFileTypes === 'true';
  tmpdir.refresh();

  fs.mkdirSync(path.join(tmpdir.path, 'd0'), { recursive: true });

  makeTree({ depth: parseInt(depth), files: parseInt(files) });

  bench.start();
  for (let i = 0; i < n; i++) {
    fs.readdirSync(path.join(tmpdir.path, 'd0'), {
      recursive: true,
      withFileTypes,
    });
  }
  bench.end(n);
}
