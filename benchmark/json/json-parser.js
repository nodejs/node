'use strict';
const { readFile } = require('node:fs/promises');
const common = require('../common.js');
const { parse } = require('node:json');
const path = require('node:path');

const configs = {
  n: [10],
  parser: ['native', 'node'],
  input: ['simple-array', 'simple-object', 'nested-ascii', 'nested-unicode', 'twitter'],
};

const bench = common.createBenchmark(main, configs);

async function main(conf) {
  const fn = conf.parser === 'native' ? JSON.parse : parse;

  const filename = `${conf.input}.json`;
  const json = await readFile(path.join(__dirname, filename), 'utf-8');

  bench.start();

  for (let i = 0; i < conf.n; i++) {
    fn(json);
  }

  bench.end(conf.n);
}
