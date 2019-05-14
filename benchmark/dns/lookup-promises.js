'use strict';

const common = require('../common.js');
const { lookup } = require('dns').promises;

const bench = common.createBenchmark(main, {
  name: ['127.0.0.1', '::1'],
  all: ['true', 'false'],
  n: [5e6]
});

function main({ name, n, all }) {
  if (all === 'true') {
    const opts = { all: true };
    bench.start();
    (async function cb() {
      for (let i = 0; i < n; i++) {
        await lookup(name, opts);
      }
    })();
    bench.end(n);
  } else {
    bench.start();
    (async function cb() {
      for (let i = 0; i < n; i++) {
        await lookup(name);
      }
    })();
    bench.end(n);
  }
}
