'use strict';
var common = require('../common.js');
var domain = require('domain');

var bench = common.createBenchmark(main, {
  arguments: [0, 1, 2, 3],
  n: [10]
});

var bdomain = domain.create();
var gargs = [1, 2, 3];

function main(conf) {

  var n = +conf.n;
  var myArguments = gargs.slice(0, conf.arguments);
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
