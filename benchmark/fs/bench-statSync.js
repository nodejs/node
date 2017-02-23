'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [1e4],
  kind: ['fstatSync', 'lstatSync', 'statSync']
});


function main(conf) {
  const n = conf.n >>> 0;
  var fn;
  var i;
  switch (conf.kind) {
    case 'statSync':
    case 'lstatSync':
      fn = fs[conf.kind];
      bench.start();
      for (i = 0; i < n; i++) {
        fn(__filename);
      }
      bench.end(n);
      break;
    case 'fstatSync':
      fn = fs.fstatSync;
      const fd = fs.openSync(__filename, 'r');
      bench.start();
      for (i = 0; i < n; i++) {
        fn(fd);
      }
      bench.end(n);
      fs.closeSync(fd);
      break;
    default:
      throw new Error('Invalid kind argument');
  }
}
