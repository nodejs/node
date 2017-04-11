'use strict';
var common = require('../common.js');
var bench = common.createBenchmark(main, {
  n: [1000]
});

var spawn = require('child_process').spawn;
function main(conf) {
  var n = +conf.n;

  bench.start();
  go(n, n);
}

function go(n, left) {
  if (--left === 0)
    return bench.end(n);

  var child = spawn('echo', ['hello']);
  child.on('exit', function(code) {
    if (code)
      process.exit(code);
    else
      go(n, left);
  });
}
