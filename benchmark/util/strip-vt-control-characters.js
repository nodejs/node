'use strict';

const common = require('../common.js');

const { stripVTControlCharacters } = require('node:util');
const assert = require('node:assert');

const bench = common.createBenchmark(main, {
  input: ['noAnsi-short', 'noAnsi-long', 'ansi-short', 'ansi-long'],
  n: [1e6],
});

function main({ input, n }) {
  let str;
  switch (input) {
    case 'noAnsi-short':
      str = 'This is a plain text string without any ANSI codes';
      break;
    case 'noAnsi-long':
      str = 'Long plain text without ANSI. '.repeat(333);
      break;
    case 'ansi-short':
      str = '\u001B[31mHello\u001B[39m';
      break;
    case 'ansi-long':
      str = ('\u001B[31m' + 'colored text '.repeat(10) + '\u001B[39m').repeat(10);
      break;
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const result = stripVTControlCharacters(str);
    assert.ok(typeof result === 'string');
  }
  bench.end(n);
}
