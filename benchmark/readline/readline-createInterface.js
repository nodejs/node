'use strict';
const common = require('../common.js');
const readline = require('readline');
const { Readable, Writable } = require('stream');

const bench = common.createBenchmark(main, {
  n: [1e5],
  terminal: [0, 1],
});

function main({ n, terminal }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    const input = new Readable({ read() {} });
    const output = new Writable({ write(chunk, encoding, callback) {
      callback();
    } });
    const rl = readline.createInterface({
      input,
      output,
      terminal: Boolean(terminal),
    });
    rl.close();
  }
  bench.end(n);
}
