'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['normal', 'destructureObject'],
  millions: [100]
});

function runNormal(n) {
  var i = 0;
  var o = { x: 0, y: 1 };
  bench.start();
  for (; i < n; i++) {
    /* eslint-disable no-unused-vars */
    var x = o.x;
    var y = o.y;
    var r = o.r || 2;
    /* eslint-enable no-unused-vars */
  }
  bench.end(n / 1e6);
}

function runDestructured(n) {
  var i = 0;
  var o = { x: 0, y: 1 };
  bench.start();
  for (; i < n; i++) {
    /* eslint-disable no-unused-vars */
    var { x, y, r = 2 } = o;
    /* eslint-enable no-unused-vars */
  }
  bench.end(n / 1e6);
}

function main(conf) {
  const n = +conf.millions * 1e6;

  switch (conf.method) {
    case 'normal':
      runNormal(n);
      break;
    case 'destructureObject':
      runDestructured(n);
      break;
    default:
      throw new Error('Unexpected method');
  }
}
