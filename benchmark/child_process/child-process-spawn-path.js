'use strict';
const common = require('../common.js');
const { spawnSync } = require('child_process');
const { basename, delimiter, dirname, join } = require('path');
const { tmpdir } = require('os');

const bench = common.createBenchmark(main, {
  n: [100],
  pathEntries: [0, 8, 32],
  file: ['path', 'absolute'],
});

function main({ n, pathEntries, file }) {
  const originalPath = process.env.PATH;
  const executable = process.execPath;
  const executableDir = dirname(executable);
  const executableName = basename(executable);
  const missingPaths = [];

  for (let i = 0; i < pathEntries; ++i) {
    missingPaths.push(join(tmpdir(), `node-spawn-path-missing-${process.pid}-${i}`));
  }

  process.env.PATH = [...missingPaths, executableDir].join(delimiter);

  const command = file === 'path' ? executableName : executable;
  const args = ['-e', ''];
  const options = { stdio: 'ignore' };

  try {
    const warmup = spawnSync(command, args, options);
    if (warmup.error)
      throw warmup.error;

    bench.start();
    for (let i = 0; i < n; ++i) {
      const result = spawnSync(command, args, options);
      if (result.error)
        throw result.error;
    }
    bench.end(n);
  } finally {
    if (originalPath === undefined)
      delete process.env.PATH;
    else
      process.env.PATH = originalPath;
  }
}
