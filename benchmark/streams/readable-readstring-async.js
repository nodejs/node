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

  text(s).then((ret) => {
    console.log(ret.length);
    bench.end(n, ret);
  });

  for (let k = 0; k < n; ++k) {
    for (let i = 0; i < 1e3; ++i)
      s.push(b);
  }
  s.push(null);
}

async function text(s) {
  s.setEncoding('utf8');
  let ret = '';
  for await (const chunk of s) {
    ret += chunk;
  }
  return ret;
}
