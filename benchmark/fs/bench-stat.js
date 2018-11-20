'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [20e4],
  statType: ['fstat', 'lstat', 'stat']
});


function main({ n, statType }) {
  var arg;
  if (statType === 'fstat')
    arg = fs.openSync(__filename, 'r');
  else
    arg = __filename;

  bench.start();
  (function r(cntr, fn) {
    if (cntr-- <= 0) {
      bench.end(n);
      if (statType === 'fstat')
        fs.closeSync(arg);
      return;
    }
    fn(arg, function() {
      r(cntr, fn);
    });
  }(n, fs[statType]));
}
