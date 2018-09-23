var common = require('../common.js');
var bench = common.createBenchmark(main, {
  thousands: [1]
});

var spawn = require('child_process').spawn;
function main(conf) {
  var len = +conf.thousands * 1000;

  bench.start();
  go(len, len);
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
