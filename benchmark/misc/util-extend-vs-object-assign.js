'use strict';

const common = require('../common.js');
const util = require('util');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  type: ['extend', 'assign'],
  n: [10e4]
});

function main(conf) {
  let fn;
  const n = conf.n | 0;
  let v8command;

  if (conf.type === 'extend') {
    fn = util._extend;
    v8command = '%OptimizeFunctionOnNextCall(util._extend)';
  } else if (conf.type === 'assign') {
    fn = Object.assign;
    // Object.assign is built-in, cannot be optimized
    v8command = '';
  }

  // Force-optimize the method to test so that the benchmark doesn't
  // get disrupted by the optimizer kicking in halfway through.
  for (var i = 0; i < conf.type.length * 10; i += 1)
    fn({}, process.env);

  v8.setFlagsFromString('--allow_natives_syntax');
  eval(v8command);

  var obj = new Proxy({}, { set: function(a, b, c) { return true; } });

  bench.start();
  for (var j = 0; j < n; j += 1)
    fn(obj, process.env);
  bench.end(n);
}
