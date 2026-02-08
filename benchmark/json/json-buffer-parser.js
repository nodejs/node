'use strict';
const { readFile } = require('node:fs/promises');
const common = require('../common.js');
const { parseFromBuffer } = require('node:json');
const path = require('node:path');
const assert = require('node:assert');

const configs = {
  n: [10, 1024],
  parser: ['native', 'node'],
  input: [
    'simple-string',
    'simple-number-array',
    'simple-string-array',
    'simple-object',
    'nested-ascii',
    'nested-unicode',
    'twitter',
  ],
};

const bench = common.createBenchmark(main, configs);

async function main(conf) {
  function parseNative(buf) {
    return JSON.parse(buf.toString());
  }

  function parseNode(buf) {
    return parseFromBuffer(buf);
  }

  const fn = conf.parser === 'native' ? parseNative : parseNode;

  const filename = `${conf.input}.json`;
  const json = await readFile(path.join(__dirname, filename));

  let deadcode;
  bench.start();
  for (let i = 0; i < conf.n; i++) {
    deadcode = fn(json);
  }
  bench.end(conf.n);

  assert.ok(deadcode);
}
