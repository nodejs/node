'use strict';
const { readFile } = require('node:fs/promises');
const common = require('../common.js');
const { parse, parseFromBuffer } = require('node:json');
const path = require('node:path');

const configs = {
  n: [1024],
  encoding: ['utf-8', 'buffer'],
};

const bench = common.createBenchmark(main, configs);

async function main(conf) {
  const fn = conf.encoding === 'buffer' ? parseFromBuffer : parse;

  const enconding = conf.encoding === 'buffer' ? undefined : conf.encoding;
  const json = await readFile(path.join(__dirname, 'twitter.json'), enconding);

  bench.start();

  for (let i = 0; i < conf.n; i++) {
    fn(json);
  }

  bench.end(conf.n);
}
