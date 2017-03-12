'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [20e4],
  kind: ['fstat', 'lstat', 'stat']
});


function main(conf) {
  const n = conf.n >>> 0;
  const kind = conf.kind;
  var arg;
  if (kind === 'fstat')
    arg = fs.openSync(__filename, 'r');
  else
    arg = __filename;

  bench.start();
  (function r(cntr, fn) {
    if (cntr-- <= 0) {
      bench.end(n);
      if (kind === 'fstat')
        fs.closeSync(arg);
      return;
    }
    fn(arg, function() {
      r(cntr, fn);
    });
  }(n, fs[kind]));
}
