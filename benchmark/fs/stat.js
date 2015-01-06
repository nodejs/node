// Call fs.stat over and over again really fast.
// Then see how many times it got called.
// Yes, this is a silly benchmark.  Most benchmarks are silly.

var path = require('path');
var common = require('../common.js');
var fs = require('fs');

var FILES = [
  require.resolve('../../lib/assert.js'),
  require.resolve('../../lib/console.js'),
  require.resolve('../../lib/fs.js')
];

var VARIANTS = {
  promise: createPromiseBasedTest(fs.stat),
  callback: createCallBackBasedTest(fs.stat),
};

var bench = common.createBenchmark(main, {
  dur: [5],
  concurrent: [1, 10, 100],
  variant: Object.keys(VARIANTS)
});

function main(conf) {
  var stat = VARIANTS[conf.variant];

  if (stat == VARIANTS.promise && !process.promisifyCore) {
    bench.start();
    bench.end(0);
    return;
  }

  var calls = 0;
  bench.start();
  setTimeout(function() {
    bench.end(calls);
  }, +conf.dur * 1000);

  var cur = +conf.concurrent;
  while (cur--) run();

  function run() {
    var p = stat(next);
    if (p) p.then(next);
  }

  function next() {
    calls++;
    run();
  }
}

function createCallBackBasedTest(stat) {
 return function runStatViaCallbacks(cb) {
    stat(FILES[0], function(err, data) {
      if (err) throw err;
      second(data);
    });

    function second() {
      stat(FILES[1], function(err, data) {
        if (err) throw err;
        third(data);
      });
    }

    function third() {
      stat(FILES[2], function(err, data) {
        if (err) throw err;
        cb(data);
      });
    }
  };
}

function createPromiseBasedTest(stat) {
  return function runStatViaPromises() {
    return stat(FILES[0])
      .then(function secondP(data) {
        return stat(FILES[1]);
      })
      .then(function thirdP(data) {
        return stat(FILES[2]);
      });
  }
}
