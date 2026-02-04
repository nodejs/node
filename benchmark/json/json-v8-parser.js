'use strict';
const { readFile } = require('node:fs/promises');
const common = require('../common.js');
const path = require('node:path');

const configs = {
  n: [1024],
};

const bench = common.createBenchmark(main, configs);

async function main(conf) {
  bench.start();

  const json = await readFile(path.join(__dirname, 'twitter.json'), 'utf-8');

  for (let i = 0; i < conf.n; i++) {
    JSON.parse(json);
  }

  bench.end(conf.n);
}
