'use strict';
const common = require('../common.js');
const domain = require('domain');

const bench = common.createBenchmark(main, {
  args: [0, 1, 2, 3],
  n: [10]
});

const bdomain = domain.create();
const gargs = [1, 2, 3];

function main({ n, args }) {
  const myArguments = gargs.slice(0, args);
  bench.start();

  bdomain.enter();
  for (var i = 0; i < n; i++) {
    if (myArguments.length >= 2) {
      const args = Array.prototype.slice.call(myArguments, 1);
      fn.apply(this, args);
    } else {
      fn.call(this);
    }
  }
  bdomain.exit();

  bench.end(n);
}

function fn(a, b, c) {
  if (!a)
    a = 1;

  if (!b)
    b = 2;

  if (!c)
    c = 3;

  return a + b + c;
}
