'use strict';
const common = require('../common.js');
const readline = require('readline');
const { Readable } = require('stream');

const bench = common.createBenchmark(main, {
  n: [1e4],
  unicodeLineSeparators: [0, 1],
});

function main({ n, unicodeLineSeparators }) {
  const line = '{"item":"data","value":42}\n';
  const chunks = Array.from({ length: 100 }, () => line);

  bench.start();
  let remaining = n;

  function run() {
    if (remaining-- === 0) {
      bench.end(n);
      return;
    }

    const input = Readable.from(chunks);
    const rl = readline.createInterface({
      input,
      unicodeLineSeparators: Boolean(unicodeLineSeparators),
    });

    rl.on('line', () => {});
    rl.on('close', run);
  }

  run();
}
