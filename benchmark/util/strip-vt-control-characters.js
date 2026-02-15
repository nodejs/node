'use strict';

const common = require('../common.js');

const { stripVTControlCharacters } = require('node:util');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  input: ['plain-short', 'plain-long', 'ansi-short', 'ansi-heavy'],
  n: [1e6],
});

function main({ input, n }) {
  let str;
  switch (input) {
    case 'plain-short':
      str = 'Hello, World!';
      break;
    case 'plain-long':
      str = 'a'.repeat(1000);
      break;
    case 'ansi-short':
      str = '\u001B[31mHello\u001B[39m';
      break;
    case 'ansi-heavy':
      str = '\u001B[1m\u001B[31m\u001B[4m' + 'x'.repeat(100) +
            '\u001B[24m\u001B[39m\u001B[22m';
      break;
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const result = stripVTControlCharacters(str);
    assert.ok(typeof result === 'string');
  }
  bench.end(n);
}
