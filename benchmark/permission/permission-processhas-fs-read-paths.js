'use strict';
const common = require('../common.js');
const path = require('path');

// process.permission.has() resolves the reference path natively (PathResolve
// -> NormalizeString in src/path.cc) before the granted-tree lookup, and string
// references are passed through from JS unchanged. This benchmark feeds it
// paths of different shapes so that the native normalization cost is visible.
const configs = {
  n: [1e5],
  pathType: [
    'normalized',
    'dot-segments',
    'dotdot',
    'deep',
    'dotdot-heavy',
  ],
};

const rootPath = path.resolve(__dirname, '../../..');

const options = {
  flags: [
    '--permission',
    `--allow-fs-read=${rootPath}`,
    '--allow-child-process',
    '--no-warnings',
  ],
};

const bench = common.createBenchmark(main, configs, options);

function makePath(pathType) {
  switch (pathType) {
    case 'normalized':
      // Already-normalized absolute path (the common case).
      return `${rootPath}/benchmark/permission/valid-file`;
    case 'dot-segments':
      // `.` segments and repeated separators that are collapsed.
      return `${rootPath}/./benchmark//./permission/./valid-file`;
    case 'dotdot':
      // A few `..` segments backtracking over the previous segments.
      return `${rootPath}/a/b/c/d/e/../../../../../valid-file`;
    case 'deep':
      // Many segments, no backtracking.
      return `${rootPath}${'/segment'.repeat(200)}/valid-file`;
    case 'dotdot-heavy':
      // Deep descent followed by an equally deep backtrack.
      return `${rootPath}${'/a'.repeat(50)}${'/..'.repeat(50)}/valid-file`;
    default:
      throw new Error(`Unknown pathType: ${pathType}`);
  }
}

function main({ n, pathType }) {
  const reference = makePath(pathType);

  bench.start();
  for (let i = 0; i < n; i++) {
    process.permission.has('fs.read', reference);
  }
  bench.end(n);
}
