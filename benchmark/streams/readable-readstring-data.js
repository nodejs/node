'use strict';

const common = require('../common');
const Readable = require('stream').Readable;

const bench = common.createBenchmark(main, {
  n: [50e2]
});

function main({ n }) {
  const b = 'some random string';
  const s = new Readable();
  function noop() {}
  s._read = noop;

  bench.start();

  let ret = '';
  s.setEncoding('utf8');
  s.on('data', (chunk) => {
    ret += chunk;
  }).on('end', () => {
    console.log(ret.length);
    bench.end(n, ret);
  });

  for (let k = 0; k < n; ++k) {
    for (let i = 0; i < 1e3; ++i)
      s.push(b);
  }
  s.push(null);
}
