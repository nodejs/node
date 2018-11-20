'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [1000]
});

const spawn = require('child_process').spawn;
function main({ n }) {
  bench.start();
  go(n, n);
}

function go(n, left) {
  if (--left === 0)
    return bench.end(n);

  const child = spawn('echo', ['hello']);
  child.on('exit', function(code) {
    if (code)
      process.exit(code);
    else
      go(n, left);
  });
}
