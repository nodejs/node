'use strict';

const common = require('../common.js');
const util = require('util');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  type: ['util._extend', 'Object.assign',
         'util._extend', 'Object.assign'],
  n: [10e4]
});

function main(params) {
  let fn;
  const n = params.n | 0;
  let v8command;

  if (params.type == 'util._extend') {
    fn = util._extend;
    v8command = '%OptimizeFunctionOnNextCall(util._extend)';

  } else if (params.type == 'Object.assign') {
    fn = Object.assign;
    //Object.assign is built-in, cannot be optimized
    v8command = '';
  }

  // Force-optimize the method to test so that the benchmark doesn't
  // get disrupted by the optimizer kicking in halfway through.
  for (let i = 0; i < params.type.length * 10; i += 1)
    fn({}, process.env);

  v8.setFlagsFromString('--allow_natives_syntax');
  eval(v8command);

  bench.start();
  for (let i = 0; i < n; i += 1)
    fn({}, process.env);
  bench.end(n);
}
