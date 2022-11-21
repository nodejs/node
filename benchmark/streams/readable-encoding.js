'use strict';

const common = require('../common');
const Readable = require('stream').Readable;

const BASE = 'hello world\n\n';

const bench = common.createBenchmark(main, {
  encoding: ['utf-8', 'latin1'],
  len: [256, 512, 1024 * 16],
  op: ['unshift', 'push'],
  n: [1e3]
});

function main({ n, encoding, len, op }) {
  const b = BASE.repeat(len);
  const s = new Readable({
    objectMode: false,
  });
  function noop() {}
  s._read = noop;

  bench.start();
  switch (op) {
    case 'unshift': {
      for (let i = 0; i < n; i++)
        s.unshift(b, encoding);
      break;
    }
    case 'push': {
      for (let i = 0; i < n; i++)
        s.push(b, encoding);
      break;
    }
  }
  bench.end(n);
}
