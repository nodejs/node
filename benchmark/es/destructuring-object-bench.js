'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['normal', 'destructureObject'],
  millions: [100],
});

function runNormal(millions) {
  var i = 0;
  const o = { x: 0, y: 1 };
  bench.start();
  for (; i < millions * 1e6; i++) {
    /* eslint-disable no-unused-vars */
    const x = o.x;
    const y = o.y;
    const r = o.r || 2;
    /* eslint-enable no-unused-vars */
  }
  bench.end(millions);
}

function runDestructured(millions) {
  var i = 0;
  const o = { x: 0, y: 1 };
  bench.start();
  for (; i < millions * 1e6; i++) {
    /* eslint-disable no-unused-vars */
    const { x, y, r = 2 } = o;
    /* eslint-enable no-unused-vars */
  }
  bench.end(millions);
}

function main({ millions, method }) {
  switch (method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'normal':
      runNormal(millions);
      break;
    case 'destructureObject':
      runDestructured(millions);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}
