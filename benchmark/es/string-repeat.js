'use strict';

const assert = require('assert');
const common = require('../common.js');

const configs = {
  n: [1e3],
  mode: ['Array', 'repeat'],
  encoding: ['ascii', 'utf8'],
  size: [1e1, 1e3, 1e6],
};

const bench = common.createBenchmark(main, configs);

function main(conf) {
  const n = +conf.n;
  const size = +conf.size;
  const character = conf.encoding === 'ascii' ? 'a' : '\ud83d\udc0e'; // '🐎'

  let str;

  if (conf.mode === 'Array') {
    bench.start();
    for (let i = 0; i < n; i++)
      str = new Array(size + 1).join(character);
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; i++)
      str = character.repeat(size);
    bench.end(n);
  }

  assert.strictEqual([...str].length, size);
}
