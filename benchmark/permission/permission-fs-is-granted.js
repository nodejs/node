'use strict';
const common = require('../common.js');
const fs = require('fs/promises');
const path = require('path');

const configs = {
  n: [1e5],
  concurrent: [1, 10],
};

const rootPath = path.resolve(__dirname, '../../..');

const options = {
  flags: [
    '--experimental-permission',
    `--allow-fs-read=${rootPath}`,
  ],
};

const bench = common.createBenchmark(main, configs, options);

const recursivelyDenyFiles = async (dir) => {
  const files = await fs.readdir(dir, { withFileTypes: true });
  for (const file of files) {
    if (file.isDirectory()) {
      await recursivelyDenyFiles(path.join(dir, file.name));
    } else if (file.isFile()) {
      process.permission.deny('fs.read', [path.join(dir, file.name)]);
    }
  }
};

async function main(conf) {
  const benchmarkDir = path.join(__dirname, '../..');
  // Get all the benchmark files and deny access to it
  await recursivelyDenyFiles(benchmarkDir);

  bench.start();

  for (let i = 0; i < conf.n; i++) {
    // Valid file in a sequence of denied files
    process.permission.has('fs.read', benchmarkDir + '/valid-file');
    // Denied file
    process.permission.has('fs.read', __filename);
    // Valid file a granted directory
    process.permission.has('fs.read', '/tmp/example');
  }

  bench.end(conf.n);
}
