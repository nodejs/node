var common = require('../common.js');

var bench = common.createBenchmark(main, {
  thousands: [500],
  type: ['depth', 'breadth']
});

function main(conf) {
  var n = +conf.thousands * 1e3;
  if (conf.type === 'breadth')
    breadth(n);
  else
    depth(n);
}

function depth(N) {
  var n = 0;
  bench.start();
  setTimeout(cb);
  function cb() {
    n++;
    if (n === N)
      bench.end(N / 1e3);
    else
      setTimeout(cb);
  }
}

function breadth(N) {
  var n = 0;
  bench.start();
  function cb() {
    n++;
    if (n === N)
      bench.end(N / 1e3);
  }
  for (var i = 0; i < N; i++) {
    setTimeout(cb);
  }
}
