'use strict';

const common = require('../common.js');
const util = require('util');

const bench = common.createBenchmark(main, {
  type: ['extend', 'assign'],
  n: [10e4]
});

function main({ n, type }) {
  // Default value for tests.
  if (type === '')
    type = 'extend';

  let fn;
  if (type === 'extend') {
    fn = util._extend;
  } else if (type === 'assign') {
    fn = Object.assign;
  }

  // Force-optimize the method to test so that the benchmark doesn't
  // get disrupted by the optimizer kicking in halfway through.
  for (var i = 0; i < type.length * 10; i += 1)
    fn({}, process.env);

  const obj = new Proxy({}, { set: function(a, b, c) { return true; } });

  bench.start();
  for (var j = 0; j < n; j += 1)
    fn(obj, process.env);
  bench.end(n);
}
