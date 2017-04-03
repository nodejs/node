'use strict';

const common = require('../common.js');

const configs = {
  n: [1e4],
  mode: ['full', 'short'],
  subject: ['properties', 'methods'],
};

const bench = common.createBenchmark(main, configs);

function main(conf) {
  const n = +conf.n;

  let obj;

  if (conf.mode === 'properties') {
    const a = 'a';
    const b = 1;
    const c = {};
    const d = () => {};

    if (conf.mode === 'full') {
      bench.start();
      for (let i = 0; i < n; i++)
        obj = { a: a, b: b, c: c, d: d };
      bench.end(n);
    } else {
      bench.start();
      for (let i = 0; i < n; i++)
        obj = { a, b, c, d };
      bench.end(n);
    }
  } else {
    if (conf.mode === 'full') {
      bench.start();
      for (let i = 0; i < n; i++)
        obj = {
          a: function a() {},
          b: function b() {},
          c: function c() {},
          d: function d() {},
        };
      bench.end(n);
    } else {
      bench.start();
      for (let i = 0; i < n; i++)
        obj = {
          a() {},
          b() {},
          c() {},
          d() {},
        };
      bench.end(n);
    }
  }

  return obj;
}
