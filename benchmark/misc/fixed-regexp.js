'use strict';

const common = require('../common.js');
const _makeStrTest = require('_http_outgoing')._makeStrTest;
const upgradeExpression = /^Upgrade$/i;
const upgradeExpressionTest = _makeStrTest(upgradeExpression);
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  type: ['regexp', 'faster'],
  field: ['Upgrade', 'Upgrade!', 'Connectionbalalalalala'],
  n: [10e5]
});

function directTest(input) {
  return upgradeExpression.test(input);
}

function fasterTest(input) {
  return upgradeExpressionTest(input);
}

function main(conf) {
  let fn;
  const n = conf.n | 0;
  const field = conf.field;
  let v8command;

  if (conf.type === 'regexp') {
    fn = directTest;
    v8command = '%OptimizeFunctionOnNextCall(directTest)';
  } else if (conf.type === 'faster') {
    fn = fasterTest;
    v8command = '%OptimizeFunctionOnNextCall(fasterTest)';
  }

  v8.setFlagsFromString('--allow_natives_syntax');
  eval(v8command);

  bench.start();
  for (var j = 0; j < n; j += 1)
    fn(field);
  bench.end(n);
}
