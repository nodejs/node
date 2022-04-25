'use strict';
const common = require('../common.js');

const configs = {
  n: [1e5],
  concurrent: [1, 10],
};

const options = { flags: ['--experimental-permission'] };

const bench = common.createBenchmark(main, configs, options);

async function main(conf) {
  bench.start();
  for (let i = 0; i < conf.n; i++) {
    process.permission.deny('fs.read', ['/home/example-file-' + i]);
  }
  bench.end(conf.n);
}
