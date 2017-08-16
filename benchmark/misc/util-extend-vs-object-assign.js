'use strict';

const common = require('../common.js');
const util = require('util');

const bench = common.createBenchmark(main, {
  type: ['extend', 'assign'],
  n: [10e4]
});

function main(conf) {
  let fn;
  const n = conf.n | 0;

  if (conf.type === 'extend') {
    fn = util._extend;
  } else if (conf.type === 'assign') {
    fn = Object.assign;
  }

  // Force-optimize the method to test so that the benchmark doesn't
  // get disrupted by the optimizer kicking in halfway through.
  for (var i = 0; i < conf.type.length * 10; i += 1)
    fn({}, process.env);

  var obj = new Proxy({}, { set: function(a, b, c) { return true; } });

  bench.start();
  for (var j = 0; j < n; j += 1)
    fn(obj, process.env);
  bench.end(n);
}
